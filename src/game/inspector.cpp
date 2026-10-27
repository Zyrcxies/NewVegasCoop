#include "game/inspector.h"

#include "coop/clock.h"
#include "game/addresses.h"
#include "game/game.h"
#include "game/mover.h"
#include "log.h"

#include "nvse/GameObjects.h"

#include <cstdio>
#include <cstring>

namespace game
{
    namespace
    {
        constexpr uint32_t kAiEveryMs = 200;      // the AI bits come from a game script: not every frame
        constexpr uint32_t kMotionEveryMs = 500;  // position and speed lines while moving
        constexpr uint32_t kMaxLinesPerSecond = 20;

        template <typename T>
        T Field(const void* object, uint32_t offset)
        {
            return *reinterpret_cast<const T*>(reinterpret_cast<const uint8_t*>(object) + offset);
        }

        // The actor's animation data, or null (far away, no body, or our map of it is wrong).
        const void* AnimDataOf(Actor* actor)
        {
            const void* process = actor->baseProcess;
            if (!process)
                return nullptr;
            using GetAnimDataFn = void*(__thiscall*)(const void*);
            uintptr_t table = *reinterpret_cast<const uintptr_t*>(process);
            auto get = *reinterpret_cast<const GetAnimDataFn*>(table + process::kGetAnimDataOffset);
            const void* anim = get(process);
            return anim && Field<const void*>(anim, process::kAnimActor) == actor ? anim : nullptr;
        }
    }

    std::string AnimGroupName(uint16_t groupId)
    {
        uint32_t index = groupId & 0xFF;
        if (index >= addr::kAnimGroupCount)
            return "?";
        auto name = *reinterpret_cast<const char* const*>(addr::kAnimGroupInfos + index * addr::kAnimGroupInfoSize);
        return name ? name : "?";
    }

    bool IsJumping(TESObjectREFR* ref)
    {
        if (!IsActor(ref))
            return false;
        const void* anim = AnimDataOf(static_cast<Actor*>(ref));
        if (!anim)
            return false;
        uint32_t index = Field<uint16_t>(anim, process::kAnimGroupIds + 2) & 0xFF;  // layer 1: movement
        if (index >= addr::kAnimGroupCount)
            return false;
        auto name = *reinterpret_cast<const char* const*>(addr::kAnimGroupInfos + index * addr::kAnimGroupInfoSize);
        return name && (std::strncmp(name, "JumpStart", 9) == 0 || std::strncmp(name, "JumpLoop", 8) == 0);
    }

    bool ReadActorState(TESObjectREFR* ref, dev::ActorState& out)
    {
        if (!IsActor(ref))
            return false;
        auto* actor = static_cast<Actor*>(ref);
        coop::Pose pose;
        if (GetPose(ref, pose))
        {
            out.x = pose.x;
            out.y = pose.y;
            out.z = pose.z;
            out.heading = pose.heading;
            out.space = pose.space;
        }
        MoverState m = ReadMover(ref);
        out.mover = MoverKindName(m.kind);
        out.flags = m.flags;
        out.askedFlags = m.asked;
        out.askedOn = m.askedOn;
        out.lifeState = LifeState(ref);
        out.action = AnimAction(ref);
        out.processLevel = actor->baseProcess ? static_cast<int>(Field<uint32_t>(actor->baseProcess, process::kLevel)) : -1;
        const void* anim = AnimDataOf(actor);
        out.animsKnown = anim != nullptr;
        for (int i = 0; i < dev::kAnimLayers; ++i)
            out.anims[i] = anim ? Field<uint16_t>(anim, process::kAnimGroupIds + 2 * i) : 0;
        return true;
    }

    bool Inspector::Watch(TESObjectREFR* ref)
    {
        dev::ActorState s;
        if (!ref || !ReadActorState(ref, s))
        {
            if (m_refId)
                LOG_INFO("Inspector: stopped following %08X", m_refId);
            m_refId = 0;
            return ref == nullptr;
        }
        s.aiKnown = AiStateBits(ref, s.ai);
        m_refId = RefId(ref);
        m_last = m_lastMotion = s;
        uint32_t now = GetTickCount();
        m_lastMotionAt = now;
        m_nextAi = now + kAiEveryMs;
        m_nextMotion = now + kMotionEveryMs;
        m_lost = false;
        LOG_INFO("Inspector: following %08X: %s", m_refId, dev::Describe(s, AnimGroupName).c_str());
        return true;
    }

    void Inspector::Forget()
    {
        m_lost = false;  // the ref id stays: it's looked up again once a world is loaded
    }

    void Inspector::Update(uint32_t now)
    {
        if (!m_refId)
            return;
        TESObjectREFR* ref = LookupRef(m_refId);
        dev::ActorState s = m_last;
        if (!ref || !ReadActorState(ref, s))
        {
            if (!m_lost)
                LOG_INFO("Inspector: %08X isn't loaded right now", m_refId);
            m_lost = true;
            return;
        }
        if (m_lost)
            LOG_INFO("Inspector: %08X is back: %s", m_refId, dev::Describe(s, AnimGroupName).c_str());
        m_lost = false;
        if (coop::IsDue(now, m_nextAi))
        {
            m_nextAi = now + kAiEveryMs;
            s.aiKnown = AiStateBits(ref, s.ai);
        }

        if (now - m_secondStart >= 1000)
        {
            if (m_linesThisSecond > kMaxLinesPerSecond)
                LOG_INFO("Inspector: (%u more changes not logged)", m_linesThisSecond - kMaxLinesPerSecond);
            m_secondStart = now;
            m_linesThisSecond = 0;
        }
        for (const std::string& line : dev::Changes(m_last, s, AnimGroupName))
            if (++m_linesThisSecond <= kMaxLinesPerSecond)
                LOG_INFO("Inspector %08X: %s", m_refId, line.c_str());

        if (coop::IsDue(now, m_nextMotion))
        {
            m_nextMotion = now + kMotionEveryMs;
            bool moved = s.x != m_lastMotion.x || s.y != m_lastMotion.y || s.z != m_lastMotion.z ||
                s.heading != m_lastMotion.heading;
            if (moved)
                LOG_INFO("Inspector %08X: %s", m_refId, dev::Motion(m_lastMotion, s, now - m_lastMotionAt).c_str());
            m_lastMotion = s;
            m_lastMotionAt = now;
        }
        m_last = s;
    }
}
