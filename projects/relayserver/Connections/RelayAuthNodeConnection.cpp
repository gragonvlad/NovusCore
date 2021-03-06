/*
# MIT License

# Copyright(c) 2018-2019 NovusCore

# Permission is hereby granted, free of charge, to any person obtaining a copy
# of this software and associated documentation files(the "Software"), to deal
# in the Software without restriction, including without limitation the rights
# to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
# copies of the Software, and to permit persons to whom the Software is
# furnished to do so, subject to the following conditions :

# The above copyright notice and this permission notice shall be included in all
# copies or substantial portions of the Software.

# THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
# IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
# FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT.IN NO EVENT SHALL THE
# AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
# LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
# OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
# SOFTWARE.
*/

#include "RelayAuthNodeConnection.h"
#include "Networking\ByteBuffer.h"
#include <Utils/DebugHandler.h>

std::unordered_map<u8, RelayMessageHandler> RelayAuthNodeConnection::InitMessageHandlers()
{
    std::unordered_map<u8, RelayMessageHandler> messageHandlers;

    messageHandlers[RELAY_CHALLENGE] =  { RELAYSTATUS_CHALLENGE,    sizeof(sRelayChallenge),    &RelayAuthNodeConnection::HandleCommandChallenge };
    messageHandlers[RELAY_PROOF]     =  { RELAYSTATUS_PROOF,        1,                          &RelayAuthNodeConnection::HandleCommandProof };

    return messageHandlers;
}
std::unordered_map<u8, RelayMessageHandler> const MessageHandlers = RelayAuthNodeConnection::InitMessageHandlers();

bool RelayAuthNodeConnection::Start()
{
    try
    {
        _socket->connect(asio::ip::tcp::endpoint(asio::ip::address::from_string(_address), _port));

        /* NODE_CHALLENGE */
        Common::ByteBuffer packet(5);
        packet.Write<u8>(0);       // Command
        packet.Write<u16>(335);    // Version
        packet.Write<u16>(12340);  // Build

        AsyncRead();
        Send(packet);
        return true;
    }
    catch (asio::system_error error)
    {
        NC_LOG_FATAL(error.what());
        return false;
    }
}

void RelayAuthNodeConnection::HandleRead()
{
    Common::ByteBuffer& byteBuffer = GetByteBuffer();
    while (byteBuffer.GetActualSize())
    {
        // Decrypt data post CHALLENGE Status
        if (_status == RELAYSTATUS_PROOF || _status == RELAYSTATUS_AUTHED)
        {
            _crypto->Decrypt(byteBuffer.GetReadPointer(), byteBuffer.GetActualSize());
        }

        u8 command = byteBuffer.GetDataPointer()[0];

        auto itr = MessageHandlers.find(command);
        if (itr == MessageHandlers.end())
        {
            byteBuffer.Clean();
            break;
        }

        // Client attempted incorrect auth step
        if (_status != itr->second.status)
        {
            Close(asio::error::shut_down);
            return;
        }

        u16 size = u16(itr->second.packetSize);
        if (byteBuffer.GetActualSize() < size)
            break;

        if (!(*this.*itr->second.handler)())
        {
            Close(asio::error::shut_down);
            return;
        }

        byteBuffer.ReadBytes(size);
    }

    AsyncRead();
}


bool RelayAuthNodeConnection::HandleCommandChallenge()
{
    _status = RELAYSTATUS_CLOSED;
    sRelayChallenge* relayChallenge = reinterpret_cast<sRelayChallenge*>(GetByteBuffer().GetReadPointer());

    _key->Bin2BN(relayChallenge->K, 32);
    _crypto->SetupClient(_key);

    /* Send fancy encrypted packet here */
    Common::ByteBuffer packet;
    packet.Write<u8>(RELAY_PROOF); // RELAY_PROOF
    _crypto->Encrypt(packet.GetReadPointer(), packet.GetActualSize());
    _status = RELAYSTATUS_PROOF;

    Send(packet);
    return true;
}

bool RelayAuthNodeConnection::HandleCommandProof()
{
    _status = RELAYSTATUS_AUTHED;


    return true;
}