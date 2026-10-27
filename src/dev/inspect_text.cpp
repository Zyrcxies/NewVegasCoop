#include "dev/inspect_text.h"

#include <cmath>
#include <cstdarg>
#include <cstdio>

namespace dev
{
    namespace
    {
        // Movement flag names (JIP LN GameObjects.h, ActorMover::MovementFlags).
        struct FlagName { uint32_t bit; const char* name; };
        constexpr FlagName kMoveFlags[] = {
            {0x1, "Forward"}, {0x2, "Backward"}, {0x4, "Left"}, {0x8, "Right"},
            {0x10, "TurnLeft"}, {0x20, "TurnRight"}, {0x40, "Keyboard"},
            {0x100, "Walking"}, {0x200, "Running"}, {0x400, "Sneaking"}, {0x800, "Swimming"},
            {0x1000, "Jump"}, {0x2000, "Flying"}, {0x4000, "Fall"}, {0x8000, "Slide"},
        };

        std::string Format(const char* fmt, ...)
        {
            char buf[512];
            va_list args;
            va_start(args, fmt);
            vsnprintf(buf, sizeof(buf), fmt, args);
            va_end(args);
            return buf;
        }

        std::string Flags(uint32_t flags) { return Format("%s (0x%X)", MoveFlagNames(flags).c_str(), flags); }

        std::string AiText(const ActorState& s)
        {
            if (!s.aiKnown)
                return "AI state unknown";
            AiBits a = UnpackAi(s.ai);
            return Format("weapon %s, %s%s%s, knocked %d, sitting %d, package type %d, procedure %d",
                a.weaponOut ? "out" : "away", a.sneaking ? "sneaking" : "standing up",
                a.inCombat ? ", in combat" : "", a.restrained ? ", restrained" : "",
                a.knocked, a.sitting, a.package, a.procedure);
        }

        // JIP LN GameProcess.h, enum AnimAction.
        const char* ActionName(int a)
        {
            static const char* const kNames[] = {"EquipWeapon", "UnequipWeapon", "Attack", "AttackEject",
                "AttackFollowThrough", "AttackThrow", "AttackThrowAttach", "Block", "Recoil", "Reload", "Stagger",
                "Dodge", "WaitForLowerBodyAnim", "WaitForSpecialIdle", "ScriptAnimation", "ReloadLoopStart",
                "ReloadLoopEnd", "ReloadLoop"};
            if (a == -1)
                return "none";
            return a >= 0 && a < static_cast<int>(sizeof(kNames) / sizeof(kNames[0])) ? kNames[a] : "?";
        }

        std::string AnimText(uint16_t id, AnimNameFn name)
        {
            return Format("%s (0x%04X)", name(id).c_str(), id);
        }

        std::string MoverText(const ActorState& s)
        {
            std::string t = Format("%s mover, moving %s", s.mover, Flags(s.flags).c_str());
            if (s.askedOn)
                t += ", told to move " + Flags(s.askedFlags);
            return t;
        }
    }

    AiBits UnpackAi(uint32_t b)
    {
        AiBits a;
        a.weaponOut = b & 1;
        a.sneaking = b & 2;
        a.inCombat = b & 4;
        a.restrained = b & 8;
        a.knocked = (b >> 4) & 7;
        a.sitting = (b >> 7) & 15;
        a.package = static_cast<int>((b >> 11) & 127) - 1;
        a.procedure = static_cast<int>((b >> 18) & 127) - 1;
        return a;
    }

    uint32_t PackAi(const AiBits& a)
    {
        return (a.weaponOut ? 1u : 0) | (a.sneaking ? 2u : 0) | (a.inCombat ? 4u : 0) | (a.restrained ? 8u : 0) |
            (static_cast<uint32_t>(a.knocked) & 7) << 4 | (static_cast<uint32_t>(a.sitting) & 15) << 7 |
            (static_cast<uint32_t>(a.package + 1) & 127) << 11 | (static_cast<uint32_t>(a.procedure + 1) & 127) << 18;
    }

    std::string MoveFlagNames(uint32_t flags)
    {
        std::string out;
        uint32_t known = 0;
        for (const FlagName& f : kMoveFlags)
        {
            known |= f.bit;
            if (flags & f.bit)
                out += (out.empty() ? "" : " ") + std::string(f.name);
        }
        if (flags & ~known)
            out += Format("%s0x%X", out.empty() ? "" : " ", flags & ~known);
        return out.empty() ? "none" : out;
    }

    std::string Describe(const ActorState& s, AnimNameFn animName)
    {
        std::string t = Format("at %.0f %.0f %.0f facing %.0f deg in %08X; life state %d; AI level %d; ",
            s.x, s.y, s.z, s.heading * 57.2957795f, s.space, s.lifeState, s.processLevel);
        t += MoverText(s) + "; " + AiText(s) + Format("; action %s; animations:", ActionName(s.action));
        if (!s.animsKnown)
            return t + " none";
        for (int i = 0; i < kAnimLayers; ++i)
            t += Format(" %d=", i) + AnimText(s.anims[i], animName);
        return t;
    }

    std::vector<std::string> Changes(const ActorState& a, const ActorState& b, AnimNameFn animName)
    {
        std::vector<std::string> out;
        if (a.space != b.space)
            out.push_back(Format("now in %08X (was %08X)", b.space, a.space));
        if (a.lifeState != b.lifeState)
            out.push_back(Format("life state %d (was %d)", b.lifeState, a.lifeState));
        if (a.processLevel != b.processLevel)
            out.push_back(Format("AI level %d (was %d)", b.processLevel, a.processLevel));
        if (std::string(a.mover) != b.mover || a.flags != b.flags || a.askedOn != b.askedOn ||
            (b.askedOn && a.askedFlags != b.askedFlags))
            out.push_back(MoverText(b) + " (was " + Flags(a.flags) + ")");
        if (a.action != b.action)
            out.push_back(Format("action %s (was %s)", ActionName(b.action), ActionName(a.action)));
        if (a.aiKnown != b.aiKnown || a.ai != b.ai)
            out.push_back(AiText(b));
        if (a.animsKnown != b.animsKnown)
            out.push_back(b.animsKnown ? "animated again" : "not animated (too far away, or no body)");
        else if (b.animsKnown)
            for (int i = 0; i < kAnimLayers; ++i)
                if (a.anims[i] != b.anims[i])
                    out.push_back(Format("animation layer %d: ", i) + AnimText(b.anims[i], animName) +
                        " (was " + AnimText(a.anims[i], animName) + ")");
        return out;
    }

    std::string Motion(const ActorState& a, const ActorState& b, uint32_t ms)
    {
        std::string t = Format("at %.0f %.0f %.0f facing %.0f deg", b.x, b.y, b.z, b.heading * 57.2957795f);
        if (ms == 0 || a.space != b.space)
            return t;
        float dx = b.x - a.x, dy = b.y - a.y, dz = b.z - a.z;
        float speed = std::sqrt(dx * dx + dy * dy) * 1000.0f / static_cast<float>(ms);
        return t + Format(", speed %.0f units/s (up %.0f)", speed, dz * 1000.0f / static_cast<float>(ms));
    }
}
