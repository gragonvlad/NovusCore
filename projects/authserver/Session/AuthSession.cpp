/*
# MIT License

# Copyright(c) 2018 NovusCore

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

#include "AuthSession.h"
#include <Networking\ByteBuffer.h>

std::unordered_map<uint8_t, AuthMessageHandler> AuthSession::InitMessageHandlers()
{
    std::unordered_map<uint8_t, AuthMessageHandler> messageHandlers;

    messageHandlers[AUTH_CHALLENGE]             =   { STATUS_CHALLENGE,         4,                              1,   &AuthSession::HandleCommandChallenge };
    messageHandlers[AUTH_PROOF]                 =   { STATUS_PROOF,             sizeof(cAuthLogonProof),        1,   &AuthSession::HandleCommandProof };
    messageHandlers[AUTH_RECONNECT_CHALLENGE]   =   { STATUS_CHALLENGE,         4,                              1,   &AuthSession::HandleCommandReconnectChallenge };
    messageHandlers[AUTH_RECONNECT_PROOF]       =   { STATUS_RECONNECT_PROOF,   sizeof(cAuthReconnectProof),    1,   &AuthSession::HandleCommandReconnectProof };
    messageHandlers[AUTH_GAMESERVER_LIST]       =   { STATUS_AUTHED,            5,                              3,   &AuthSession::HandleCommandGameServerList };

    return messageHandlers;
}
std::unordered_map<uint8_t, AuthMessageHandler> const MessageHandlers = AuthSession::InitMessageHandlers();

bool AuthSession::Start()
{
    AsyncRead();
    return true;
}

void AuthSession::HandleRead()
{
    Common::ByteBuffer& byteBuffer = GetByteBuffer();
    ResetPacketsReadThisRead();

    while (byteBuffer.GetActualSize())
    {
        uint8_t command = byteBuffer.GetDataPointer()[0];

        auto itr = MessageHandlers.find(command);
        if (itr == MessageHandlers.end())
        {
            byteBuffer.Clean();
            break;
        }

        // Client attempted incorrect auth step
        if (_status != itr->second.status)
        {
            _socket->close();
            return;
        }

        if (packetsReadThisRead[command] == itr->second.maxPacketsPerRead)
        {
            _socket->close();
            return;
        }
        else
        {
            ++packetsReadThisRead[command];
        }

        uint16_t size = uint16_t(itr->second.packetSize);
        if (byteBuffer.GetActualSize() < size)
            break;

        if (command == AUTH_CHALLENGE || command == AUTH_RECONNECT_CHALLENGE)
        {
            cAuthLogonChallenge* logonChallenge = reinterpret_cast<cAuthLogonChallenge*>(byteBuffer.GetReadPointer());
            size += logonChallenge->size;
            if (size > (sizeof(cAuthLogonChallenge) + 16))
            {
                _socket->close();
                return;
            }
        }

        if (byteBuffer.GetActualSize() < size)
            break;

        if (!(*this.*itr->second.handler)())
        {
            _socket->close();
            return;
        }

        byteBuffer.ReadBytes(size);
    }

    AsyncRead();
}

bool AuthSession::HandleCommandChallenge()
{
    _status = STATUS_CLOSED;

    cAuthLogonChallenge* logonChallenge = reinterpret_cast<cAuthLogonChallenge*>(GetByteBuffer().GetReadPointer());

    std::string login((char const*)logonChallenge->username_pointer, logonChallenge->username_length);

    Common::ByteBuffer pkt;
    pkt.Write(uint8_t(0));//AUTH_LOGON_CHALLENGE);
    pkt.Write(uint8_t(0x00));

    // Check if account exist in DB if so, grab v and s

    username = login;
    std::string databaseV = "18CA3F48A75E879D83959E12AFEEA682A8C8EADC20582107F3721F8AE44400CD";
    std::string databaseS = "C55B9889E9CC8DE96F8A3D0B2D54C6B39AEF58E2C50C816660E7FB77802E760B";

    s.Hex2BN(databaseS.c_str());
    v.Hex2BN(databaseV.c_str());

    b.Rand(19 * 8);
    BigNumber gen = g.ModExponential(b, N);
    B = ((v * 3) + gen) % N;

    assert(gen.GetBytes() <= 32);

    BigNumber _unkNum;
    _unkNum.Rand(16 * 8);

    /* Check Build Version Here */
    pkt.Write(0x00); // WOW_SUCCESSS
    _status = STATUS_PROOF;

    pkt.Append(B.BN2BinArray(32).get(), 32);
    pkt.Write(1);
    pkt.Append(g.BN2BinArray(1).get(), 1);
    pkt.Write(32);
    pkt.Append(N.BN2BinArray(32).get(), 32);
    pkt.Append(s.BN2BinArray(int32_t(0x20)).get(), size_t(0x20));   // 32 bytes (SRP_6_S)
    pkt.Append(_unkNum.BN2BinArray(16).get(), 16);
    
    //uint8 securityFlags = 0;
    pkt.Write(0);

    Send(pkt);
    return true;
}

bool AuthSession::HandleCommandProof()
{
    _status = STATUS_CLOSED;

    cAuthLogonProof* logonProof = reinterpret_cast<cAuthLogonProof*>(GetByteBuffer().GetReadPointer());

    BigNumber A;
    A.Bin2BN(logonProof->A, 32);

    // SRP safeguard: abort if A == 0
    if ((A % N).IsZero())
        return false;

    SHA1Hasher sha;
    sha.UpdateHashForBn(2, &A, &B);
    sha.Finish();

    BigNumber u;
    u.Bin2BN(sha.GetData(), 20);
    BigNumber S = (A * (v.ModExponential(u, N))).ModExponential(b, N);

    uint8_t t[32];
    uint8_t t1[16];
    memcpy(t, S.BN2BinArray(32).get(), 32);

    for (int i = 0; i < 16; ++i)
        t1[i] = t[i * 2];

    sha.Init();
    sha.UpdateHash(t1, 16);
    sha.Finish();

    uint8_t vK[40];
    for (int i = 0; i < 20; ++i)
        vK[i * 2] = sha.GetData()[i];

    for (int i = 0; i < 16; ++i)
        t1[i] = t[i * 2 + 1];

    sha.Init();
    sha.UpdateHash(t1, 16);
    sha.Finish();

    for (int i = 0; i < 20; ++i)
        vK[i * 2 + 1] = sha.GetData()[i];
    K.Bin2BN(vK, 40);

    sha.Init();
    sha.UpdateHashForBn(1, &N);
    sha.Finish();

    uint8_t hash[20];
    memcpy(hash, sha.GetData(), 20);
    sha.Init();
    sha.UpdateHashForBn(1, &g);
    sha.Finish();

    for (int i = 0; i < 20; ++i)
        hash[i] ^= sha.GetData()[i];

    sha.Init();
    sha.UpdateHash("NIXIFY");
    sha.Finish();

    BigNumber t3;
    t3.Bin2BN(hash, 20);
    uint8_t t4[SHA_DIGEST_LENGTH];
    memcpy(t4, sha.GetData(), SHA_DIGEST_LENGTH);

    sha.Init();
    sha.UpdateHashForBn(1, &t3);
    sha.UpdateHash(t4, SHA_DIGEST_LENGTH);
    sha.UpdateHashForBn(4, &s, &A, &B, &K);
    sha.Finish();

    BigNumber M;
    M.Bin2BN(sha.GetData(), sha.GetLength());
    if (!memcmp(M.BN2BinArray(sha.GetLength()).get(), logonProof->M1, 20))
    {
        printf("Proof Matches\n");
        // Finish SRP6 and send the final result to the client
        sha.Init();
        sha.UpdateHashForBn(3, &A, &M, &K);
        sha.Finish();

        // Update Database with SessionKey

        Common::ByteBuffer packet;
        sAuthLogonProof proof;

        memcpy(proof.M2, sha.GetData(), 20);
        proof.cmd = AUTH_PROOF;
        proof.error = 0;
        proof.AccountFlags = 0x00800000;    // 0x01 = GM, 0x08 = Trial, 0x00800000 = Pro pass (arena tournament)
        proof.SurveyId = 0;
        proof.unk3 = 0;

        packet.Resize(sizeof(proof));
        std::memcpy(packet.data(), &proof, sizeof(proof));
        packet.WriteBytes(sizeof(proof));

        Send(packet);
        _status = STATUS_AUTHED;
    }
    else
    {
        printf("Proof Does not match\n");

        Common::ByteBuffer byteBuffer;
        byteBuffer.Write(AUTH_PROOF);
        byteBuffer.Write(4); // error
        byteBuffer.Write(3); // AccountFlag
        byteBuffer.Write(0);
        Send(byteBuffer);
    }

    return true;
}

bool AuthSession::HandleCommandReconnectChallenge()
{
    return false;
    /*Common::ByteBuffer pkt;
    pkt.Write(AUTH_RECONNECT_CHALLENGE);

    // Check if account exists
    /*if (login != "NIXIFY")
    {
        pkt.Write(uint8_t(0x03)); //WOW_FAIL_UNKNOWN_ACCOUNT);
        Send(pkt);
        return true;
    }

    _reconnectProof.Rand(16 * 8);
    _status = STATUS_RECONNECT_PROOF;
    
    pkt.Write(0); // WOW_SUCCESS
    pkt.Append(_reconnectProof.BN2BinArray(16).get(), 16);  // 16 bytes random
    uint64_t zeros = 0x00;
    pkt.Append((uint8_t*)&zeros, sizeof(zeros));                 // 8 bytes zeros
    pkt.Append((uint8_t*)&zeros, sizeof(zeros));                 // 8 bytes zeros

    Send(pkt);*/
}

bool AuthSession::HandleCommandReconnectProof()
{
    _status = STATUS_CLOSED;
    cAuthReconnectProof *reconnectLogonProof = reinterpret_cast<cAuthReconnectProof*>(GetByteBuffer().GetReadPointer());
    
    if (!_reconnectProof.GetBytes() || !K.GetBytes())
        return false;

    BigNumber t1;
    t1.Bin2BN(reconnectLogonProof->R1, 16);

    SHA1Hasher sha;
    sha.Init();
    sha.UpdateHash("NIXIFY");
    sha.UpdateHashForBn(3, &t1, &_reconnectProof, &K);
    sha.Finish();

    if (!memcmp(sha.GetData(), reconnectLogonProof->R2, SHA_DIGEST_LENGTH))
    {
        // Sending response
        Common::ByteBuffer pkt;
        pkt.Write(AUTH_RECONNECT_PROOF);
        pkt.Write(0x00);
        uint16_t unk1 = 0x00;
        pkt.Append((uint8_t*)&unk1, sizeof(unk1));  // 2 bytes zeros
        Send(pkt);
        _status = STATUS_AUTHED;
        return true;
    }

    return false;
}

bool AuthSession::HandleCommandGameServerList()
{
    Common::ByteBuffer bytebuffer = GetByteBuffer();
    printf("HandleCommandGameServerList\n");
    _status = STATUS_WAITING_FOR_GAMESERVER;

    Common::ByteBuffer pkt;

    uint16_t RealmListSize = 1;
    float population = 0;

    /* Test Packet */
    pkt.Write(1); // Realm Type
    pkt.Write(0); // Realm Locked (Only needed for clients TBC+)
    pkt.Write(0); // Realm Flag

    pkt.Write("[NovusCore] Internal Realm"); // Realm Name
    pkt.Write("127.0.0.1:8085"); // Realm IP/Port
    pkt.Append((uint8_t*)&population, sizeof(population)); // Realm Population Level

    pkt.Write(9); // Characters Count
    pkt.Write(1); // Timezone
    pkt.Write(1); // Realm Id
    
    // (Only needed for clients TBC+)
    /*pkt.Write(3); // Major Version
    pkt.Write(3); // Minor Version
    pkt.Write(5); // Bugfix Version
    uint16_t build = 12340;
    pkt.Append((uint8_t*)&build, sizeof(build)); // Build*/
    

    // (Only needed for clients TBC+)
    pkt.Write(0x10); // Unk1
    pkt.Write(0x00); // Unk2

    Common::ByteBuffer RealmListSizeBuffer;

    uint32_t unk1 = 0;
    RealmListSizeBuffer.Append((uint8_t*)&unk1, sizeof(unk1));

    uint16_t realmsCount = RealmListSize;
    RealmListSizeBuffer.Append((uint8_t*)&realmsCount, sizeof(realmsCount));

    Common::ByteBuffer hdr;
    hdr.Write(AUTH_GAMESERVER_LIST);
    
    uint16_t combinedSize = pkt.GetActualSize() + RealmListSizeBuffer.GetActualSize();
    hdr.Append((uint8_t*)&combinedSize, sizeof(combinedSize));
    hdr.Append(RealmListSizeBuffer);
    hdr.Append(pkt);
    Send(hdr);

    _status = STATUS_AUTHED;

    return true;
}

/*
void AsyncRead()
{
    if (!IsOpen())
    return;

    _readBuffer.Normalize();
    _readBuffer.EnsureFreeSpace();
    _socket.async_read_some(boost::asio::buffer(_readBuffer.GetWritePointer(), _readBuffer.GetRemainingSpace()),
    std::bind(&Socket<T>::ReadHandlerInternal, this->shared_from_this(), std::placeholders::_1, std::placeholders::_2));
}
*/