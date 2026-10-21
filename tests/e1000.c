#include <assert.h>
#include <stdio.h>
#include "net/e1000.h"

/* Receive-descriptor validation, independent of hardware. */
int main(void)
{
    bool discarding = false;
    assert(e1000_rx_classify(60, E1000_RXD_DD | E1000_RXD_EOP, 0, &discarding) == E1000_RX_DELIVER);
    assert(e1000_rx_classify(E1000_FRAME_MIN, E1000_RXD_EOP, 0, &discarding) == E1000_RX_DELIVER);
    assert(e1000_rx_classify(E1000_FRAME_MAX, E1000_RXD_EOP, 0, &discarding) == E1000_RX_DELIVER);
    assert(e1000_rx_classify(E1000_FRAME_MIN - 1, E1000_RXD_EOP, 0, &discarding) == E1000_RX_MALFORMED);
    assert(e1000_rx_classify(E1000_FRAME_MAX + 1, E1000_RXD_EOP, 0, &discarding) == E1000_RX_MALFORMED);
    assert(e1000_rx_classify(0, E1000_RXD_EOP, 0, &discarding) == E1000_RX_MALFORMED);
    assert(e1000_rx_classify(UINT16_MAX, E1000_RXD_EOP, 0, &discarding) == E1000_RX_MALFORMED);
    for (unsigned bit = 0; bit < 8; ++bit)
        assert(e1000_rx_classify(100, E1000_RXD_EOP, (uint8_t)(1u << bit), &discarding) == E1000_RX_MALFORMED);
    assert(!discarding);
    /* A frame spanning descriptors is dropped whole and counted once, even if
     * its final piece looks valid; the next frame is unaffected. */
    assert(e1000_rx_classify(2048, E1000_RXD_DD, 0, &discarding) == E1000_RX_CONTINUE && discarding);
    assert(e1000_rx_classify(2048, E1000_RXD_DD, 0, &discarding) == E1000_RX_CONTINUE && discarding);
    assert(e1000_rx_classify(60, E1000_RXD_EOP, 0, &discarding) == E1000_RX_MALFORMED && !discarding);
    assert(e1000_rx_classify(60, E1000_RXD_EOP, 0, &discarding) == E1000_RX_DELIVER);
    puts("PASS: e1000 receive length, error and multi-descriptor validation");
    return 0;
}
