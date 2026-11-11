#include <assert.h>
#include <stdio.h>
#include "../src/esp_bt0_probe_policy.h"

static EspBt0Mem m(unsigned h, unsigned l, unsigned d, unsigned ld) {
    EspBt0Mem x = {h, l, d, ld};
    return x;
}

int main(void) {
    const EspBt0Mem pre = m(65536, 32768, 49152, 24576);
    const EspBt0Mem post = m(57344, 24576, 40960, 16384);
    assert(EspBt0_preflight(pre));
    assert(EspBt0_postflight(post));
    assert(!EspBt0_preflight(m(65535,32768,49152,24576)));
    assert(!EspBt0_preflight(m(65536,32767,49152,24576)));
    assert(!EspBt0_preflight(m(65536,32768,49151,24576)));
    assert(!EspBt0_preflight(m(65536,32768,49152,24575)));
    assert(!EspBt0_postflight(m(57343,24576,40960,16384)));
    assert(!EspBt0_postflight(m(57344,24575,40960,16384)));
    assert(!EspBt0_postflight(m(57344,24576,40959,16384)));
    assert(!EspBt0_postflight(m(57344,24576,40960,16383)));
    puts("BT0 host policy PASS: inclusive boundaries and all four fail-closed guards");
    return 0;
}
