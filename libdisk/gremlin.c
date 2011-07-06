/******************************************************************************
 * disk/gremlin.c
 * 
 * Custom format as used by various Gremlin Graphics releases:
 *   Lotus I, II, and III
 *   Harlequin
 * 
 * Written in 2011 by Keir Fraser
 * 
 * RAW TRACK LAYOUT:
 *  u16 0x4489,0x4489,0x4489
 *  u16 0x5555
 *  u16 data[12*512/2]
 *  u16 csum
 *  u16 trk
 *  Checksum is sum of all decoded words
 *  Sides 0 and 1 of disk are inverted from normal.
 * MFM encoding:
 *  Alternating odd/even words
 * 
 * TRKTYP_gremlin data layout:
 *  u8 sector_data[12][512]
 */

#include <libdisk/util.h>
#include "private.h"

#include <arpa/inet.h>

static void *gremlin_write_mfm(
    struct disk *d, unsigned int tracknr, struct stream *s)
{
    struct track_info *ti = &d->di->track[tracknr];
    uint16_t *block = NULL;
    unsigned int i;

    while ( (stream_next_bit(s) != -1) && !block )
    {
        uint16_t mfm[2], csum = 0, trk;
        uint32_t idx_off = s->index_offset - 15;

        if ( (uint16_t)s->word != 0x4489 )
            continue;
        if ( stream_next_bits(s, 32) == -1 )
            goto done;
        if ( s->word != 0x44894489 )
            continue;
        if ( stream_next_bits(s, 16) == -1 )
            goto done;
        if ( (uint16_t)s->word != 0x5555 )
            continue;

        ti->data_bitoff = idx_off;

        block = memalloc(ti->len);

        for ( i = 0; i < ti->nr_sectors*ti->bytes_per_sector/2; i++ )
        {
            if ( stream_next_bytes(s, mfm, sizeof(mfm)) == -1 )
                goto done;
            block[i] = (mfm[0] & 0x5555u) | ((mfm[1] & 0x5555u) << 1);
            csum += ntohs(block[i]);
        }

        if ( stream_next_bytes(s, mfm, sizeof(mfm)) == -1 )
            goto done;
        csum -= ntohs((mfm[0] & 0x5555u) | ((mfm[1] & 0x5555u) << 1));

        if ( stream_next_bytes(s, mfm, sizeof(mfm)) == -1 )
            goto done;
        trk = ntohs((mfm[0] & 0x5555u) | ((mfm[1] & 0x5555u) << 1));

        if ( (csum != 0) || (tracknr != (trk^1)) )
        {
            memfree(block);
            block = NULL;
        }
    }

done:
    if ( block != NULL )
        ti->valid_sectors = (1u << ti->nr_sectors) - 1;

    return block;
}

static void gremlin_read_mfm(
    struct disk *d, unsigned int tracknr, struct track_buffer *tbuf)
{
    struct track_info *ti = &d->di->track[tracknr];
    uint16_t csum = 0, *dat = (uint16_t *)ti->dat;
    unsigned int i;

    tbuf_bits(tbuf, SPEED_AVG, TB_raw, 32, 0x44894489);
    tbuf_bits(tbuf, SPEED_AVG, TB_raw, 32, 0x44895555);

    for ( i = 0; i < ti->nr_sectors*ti->bytes_per_sector/2; i++ )
    {
        csum += ntohs(dat[i]);
        tbuf_bits(tbuf, SPEED_AVG, TB_odd_even, 16, ntohs(dat[i]));
    }

    tbuf_bits(tbuf, SPEED_AVG, TB_odd_even, 16, csum);
    tbuf_bits(tbuf, SPEED_AVG, TB_odd_even, 16, tracknr^1);
}

struct track_handler gremlin_handler = {
    .type = TRKTYP_gremlin,
    .bytes_per_sector = 12*512,
    .nr_sectors = 1,
    .write_mfm = gremlin_write_mfm,
    .read_mfm = gremlin_read_mfm
};
