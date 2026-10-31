// New Vegas Co-op by Zyrcxies (https://github.com/Zyrcxies/NewVegasCoop). Reusing this code? See LICENSE: credit and the same license are required.

#pragma once

#include "net/transport.h"

#include <memory>

namespace net
{
    class Both : public Transport
    {
    public:
        static constexpr PeerId kSecond = 0x40000000;

        Both(Transport& first, std::unique_ptr<Transport> second) : m_first(first), m_second(std::move(second)) {}

        void Send(PeerId peer, Bytes data, bool reliable = true) override
        {
            peer & kSecond ? m_second->Send(peer & ~kSecond, std::move(data), reliable)
                           : m_first.Send(peer, std::move(data), reliable);
        }
        void Disconnect(PeerId peer) override
        {
            peer & kSecond ? m_second->Disconnect(peer & ~kSecond) : m_first.Disconnect(peer);
        }
        bool Poll(Event& out) override
        {
            m_secondFirst = !m_secondFirst;
            for (int i = 0; i < 2; ++i)
                if ((m_secondFirst == (i == 0)) ? PollSecond(out) : m_first.Poll(out))
                    return true;
            return false;
        }
        void Stop() override
        {
            m_first.Stop();
            m_second->Stop();
        }
        bool Running() const override { return m_first.Running() || m_second->Running(); }
        uint32_t RoundTripMs(PeerId peer) const override
        {
            return peer & kSecond ? m_second->RoundTripMs(peer & ~kSecond) : m_first.RoundTripMs(peer);
        }

    private:
        bool PollSecond(Event& out)
        {
            if (!m_second->Poll(out))
                return false;
            out.peer |= kSecond;
            return true;
        }

        Transport& m_first;
        std::unique_ptr<Transport> m_second;
        bool m_secondFirst = false;
    };
}
