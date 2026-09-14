#include "../voice_controller.h"
#include "../windows_ipc.h"
#include <cassert>
#include <limits>

int main()
{
    using namespace FanyImeVoiceController;
    Request request;
    request.controller_id = (uint64_t{7} << 32) | 1;
    request.request_id = 1;
    assert(valid_request(request, sizeof(Request)));
    request.operation = Operation::Start;
    assert(!valid_request(request, sizeof(Request)));
    request.language_bytes = 5;
    assert(valid_request(request, sizeof(Request) + 5));
    request.session_id = 2;
    assert(!valid_request(request, sizeof(Request) + 5));
    request.operation = Operation::Poll;
    request.language_bytes = 0;
    assert(valid_request(request, sizeof(Request)));
    for (const auto operation : {Operation::Stop, Operation::Cancel})
    {
        request.operation = operation;
        assert(valid_request(request, sizeof(Request)));
    }
    request.reserved = 1;
    assert(!valid_request(request, sizeof(Request)));
    request.reserved = 0;
    request.version = 1;
    assert(!valid_request(request, sizeof(Request)));
    request.version = Version;
    request.language_bytes = std::numeric_limits<uint32_t>::max();
    assert(!valid_request(request, sizeof(Request)));
    request.language_bytes = 0;
    request.controller_id = 7;
    assert(!valid_request(request, sizeof(Request)));

    Reply reply;
    reply.request_id = 1;
    assert(valid_reply(reply, sizeof(Reply)));
    reply.phase = Phase::Recording;
    assert(!valid_reply(reply, sizeof(Reply)));
    reply.session_id = 2;
    reply.level = MaxLevel;
    assert(valid_reply(reply, sizeof(Reply)));
    reply.level++;
    assert(!valid_reply(reply, sizeof(Reply)));
    reply.level = 0;
    reply.phase = Phase::Complete;
    reply.text_bytes = MaxTextBytes;
    assert(valid_reply(reply, sizeof(Reply) + MaxTextBytes));
    assert(!valid_reply(reply, sizeof(Reply) + MaxTextBytes + 1));
    reply.status = Status::Stale;
    assert(!valid_reply(reply, sizeof(Reply) + MaxTextBytes));
    reply.text_bytes = 0;
    assert(valid_reply(reply, sizeof(Reply)));
    // The new endpoint does not reinterpret released v1 wire layouts.
    static_assert(sizeof(FanyImeVoiceControlHello) == 16);
    static_assert(FanyImeVoiceControl::Start == 1 && FanyImeVoiceControl::Cancel == 3);
}
