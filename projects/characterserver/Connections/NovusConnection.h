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
#pragma once

#include <asio\ip\tcp.hpp>
#include <Networking\BaseSocket.h>
#include <Networking\TcpServer.h>
#include <Cryptography\BigNumber.h>
#include <Cryptography\StreamCrypto.h>
#include <unordered_map>

enum NovusCommand
{
    NOVUS_CHALLENGE         = 0x00,
    NOVUS_PROOF             = 0x01,
    NOVUS_FOWARDPACKET      = 0x02
};
enum NovusStatus
{
    NOVUSSTATUS_CHALLENGE   = 0x0,
    NOVUSSTATUS_PROOF       = 0x1,
    NOVUSSTATUS_AUTHED      = 0x2,
    NOVUSSTATUS_CLOSED      = 0x3
};

#pragma pack(push, 1)
struct sNovusChallenge
{
    uint8_t command;
    uint8_t K[32];
};

struct NovusHeader
{
    uint8_t     command;
    uint32_t    account;
    uint16_t    opcode;
    uint16_t    size;

    void Read(Common::ByteBuffer& buffer)
    {
        buffer.Read<uint8_t>(command);
        buffer.Read<uint32_t>(account);
        buffer.Read<uint16_t>(opcode);
        buffer.Read<uint16_t>(size);
    }

    void AddTo(Common::ByteBuffer& buffer)
    {
        buffer.Append((uint8_t*)this, sizeof(NovusHeader));
    }
};

struct cCharacterCreateData
{
    std::string charName;
    uint8_t charRace;
    uint8_t charClass;
    uint8_t charGender;
    uint8_t charSkin;
    uint8_t charFace;
    uint8_t charHairStyle;
    uint8_t charHairColor;
    uint8_t charFacialStyle;
    uint8_t charOutfitId;

    void Read(Common::ByteBuffer& buffer)
    {
        buffer.Read(charName);
        buffer.Read<uint8_t>(charRace);
        buffer.Read<uint8_t>(charClass);
        buffer.Read<uint8_t>(charGender);
        buffer.Read<uint8_t>(charSkin);
        buffer.Read<uint8_t>(charFace);
        buffer.Read<uint8_t>(charHairStyle);
        buffer.Read<uint8_t>(charHairColor);
        buffer.Read<uint8_t>(charFacialStyle);
        buffer.Read<uint8_t>(charOutfitId);
    }
};
#pragma pack(pop)

struct NovusMessageHandler;
class NovusConnection : Common::BaseSocket
{
public:
    static std::unordered_map<uint8_t, NovusMessageHandler> InitMessageHandlers();

    NovusConnection(asio::ip::tcp::socket* socket, std::string address, uint16_t port, uint8_t realmId) : Common::BaseSocket(socket), _status(NOVUSSTATUS_CHALLENGE), _crypto(), _address(address), _port(port), _realmId(realmId), _headerBuffer(), _packetBuffer()
    { 
        _crypto = new StreamCrypto();
        _key = new BigNumber();

        _headerBuffer.Resize(sizeof(NovusHeader));
    }

    bool Start() override;
    void HandleRead() override;

    bool HandleCommandChallenge();
    bool HandleCommandProof();
    bool HandleCommandForwardPacket();

    void SendPacket(Common::ByteBuffer& packet);

    NovusStatus _status;
private:
    std::string _address;
    uint16_t _port;
    uint8_t _realmId;

    StreamCrypto* _crypto;
    BigNumber* _key;

    Common::ByteBuffer _headerBuffer;
    Common::ByteBuffer _packetBuffer;
};

#pragma pack(push, 1)
struct NovusMessageHandler
{
    NovusStatus status;
    size_t packetSize;
    bool (NovusConnection::*handler)();
};
#pragma pack(pop)