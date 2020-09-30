/*
 File: ContFramePool.C

 Author: Ankur Kunder
 Date  : 09/20/2020

 */

/*--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/

#define KB * (0x1 << 10)

/*--------------------------------------------------------------------------*/
/* INCLUDES */
/*--------------------------------------------------------------------------*/

#include "cont_frame_pool.H"
#include "console.H"
#include "utils.H"
#include "assert.H"

/*--------------------------------------------------------------------------*/
/* DATA STRUCTURES */
/*--------------------------------------------------------------------------*/

ContFramePool * ContFramePool::list_head = NULL;

/*--------------------------------------------------------------------------*/
/* CONSTANTS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* FORWARDS */
/*--------------------------------------------------------------------------*/

/* -- (none) -- */

/*--------------------------------------------------------------------------*/
/* METHODS FOR CLASS   C o n t F r a m e P o o l */
/*--------------------------------------------------------------------------*/

ContFramePool::ContFramePool(unsigned long _base_frame_no,
                             unsigned long _n_frames,
                             unsigned long _info_frame_no,
                             unsigned long _n_info_frames)
{
    base_frame_no = _base_frame_no;
    nframes = _n_frames;
    n_free_frames = _n_frames;
    info_frame_no = _info_frame_no;
    n_info_frames = _n_info_frames;

    // If _info_frame_no is zero then we keep management info in the first
    // frame, else we use the provided frame(s) to keep management info
    if (info_frame_no == 0) {
        // Bitmap must fit in a single frame, each frame needs 2 bits
        assert(_n_frames * 2 <= FRAME_SIZE * 8);
        bitmap = (unsigned short *) (base_frame_no * FRAME_SIZE);  // unsigned short * bitmap;
    } else {
        assert(_n_frames * 2 <= FRAME_SIZE * 8 * _n_info_frames);
        bitmap = (unsigned short *) (info_frame_no * FRAME_SIZE);
    }

    // Number of frames must be "fill" the bitmap!
    assert ((nframes % (8 * 2)) == 0);

    // Everything ok. Proceed to mark all bits in the bitmap to be FREE
    memsetw(bitmap, 0xFFFF, _n_frames / 8);
    for (unsigned int i = 0; i < _n_frames / 8; i++) {
        assert(bitmap[i] == 0xFFFF);
    }

    // Mark the first frame as being used if it is being used
    // For efficiency purpose, the first byte is used for head-of-sequence
    // bit (0 means head), the second byte is used for free bit (0 means
    // allocated, 1 means free), note that head-of-sequence's free bit will
    // be cleared as 0 to indicate allocated.
    if (_info_frame_no == 0) {
        bitmap[0] = 0x7F7F;
        n_free_frames--;
    }

    // link it in the list
    if (list_head == NULL) {
        list_head = this;
    } else {
        //WRITE THIS YOURSELF
        ContFramePool* curr = ContFramePool::list_head;
        while (curr->next_pool) {
            curr = curr->next_pool;
        }
        curr->next_pool = this;
    }
    next_pool = NULL; // pointer to the next pool

    Console::puts("Frame Pool initialized\n");
}

unsigned long ContFramePool::get_frames(unsigned int _n_frames)
{
    // Any free frames left to allocate?
    assert(n_free_frames > 0);

    // If not enough frames, return 0
    if (_n_frames > n_free_frames) {
        Console::puts("Not enough free frames\n");
        return 0;
    }

#ifdef DEBUG_FRAMEPOOL
    Console::puts("Requesting ");
    Console::puti(_n_frames);
    Console::puts(" frames\n");
#endif
    unsigned long head_frame_no = 0;

    unsigned short free_bit_mask = 0xFF;
    unsigned short mask = 0x80;
    unsigned int bitmap_index = 0;
    unsigned int i = 0;
    unsigned int offset = 0;
    unsigned int new_offset = 0;
    unsigned int n_cont_free_frames = 0;

    // find the head of first free frame sequence (base_frame_no + bitmap_index * 8 + offset),
    // the above check should guaranttee we can find a free frame
    while ((bitmap[i] & free_bit_mask) == 0x0) {
        i++;
    }
    bitmap_index = i;
    while ((bitmap[i] & mask) == 0) {
        mask >>= 1;
        offset++;
    }
    while (i * 8 < nframes) {  // each index is used for 8 frames
        while ((bitmap[i] & mask) != 0) {
            mask >>= 1;
            n_cont_free_frames++;
        }

        // check if find enough contiguous free frames
        if (n_cont_free_frames >= _n_frames) {
            break;
        }

        if (mask == 0) {
            i++;
            while (((bitmap[i] & free_bit_mask) == 0xFF) && (i * 8 < nframes)) {
                i++;
                n_cont_free_frames += 8;
                // check if find enough contiguous free frames
                if (n_cont_free_frames >= _n_frames) {
                    break;
                }
            }
            if (i * 8 >= nframes) {
                return 0;
            }
            // check if find enough contiguous free frames or cross boundary
            // WRITE THIS YOURSELF

            // pass check, there is allocated frame(s) in this bitmap element
            mask = 0x80;
            while ((bitmap[i] & mask) != 0) {
                mask >>= 1;
                n_cont_free_frames++;
            }

            // check if find enough contiguous free frames
            if (n_cont_free_frames >= _n_frames) {
                break;
            }
        }

        // find next head of free frame sequence
        assert(mask != 0);
        n_cont_free_frames = 0;
        mask = 0x80;
        while ((bitmap[i] & mask) != 0) {
            mask >>= 1;
            offset++;
        }
        assert(mask != 0);
        while (((bitmap[i] & mask) == 0) && mask != 0) {
            mask >>= 1;
            offset++;
        }

        // check if can find the next head in this bitmap element
        if (offset == 8) {
            i++;
            while (((bitmap[i] & free_bit_mask) == 0x0) && (i * 8 < nframes)) {
                i++;
            }
            if (i * 8 >= nframes) {
                break;
            }
            bitmap_index = i;
            mask = 0x80;
            offset = 0;
            while ((bitmap[i] & mask) == 0) {
                mask >>= 1;
                offset++;
            }
        } else {
            bitmap_index = i;
        }
    }

    if (n_cont_free_frames >= _n_frames) {
        unsigned int nbitmap_blocks = 1;
        if ((_n_frames - (8 - offset)) > 0) {
            nbitmap_blocks += (_n_frames - (8 - offset) + 8) / 8;
        }
        head_frame_no = base_frame_no + bitmap_index * 8 + offset;
#ifdef DEBUG_FRAMEPOOL
        Console::putui(_n_frames);
        Console::puts(" consecutive frames found, head: ");
        Console::puti(head_frame_no);
        Console::puts(", base_frame_no: ");
        Console::puti(base_frame_no);
        Console::puts(", offset: ");
        Console::puti(offset);
        Console::puts(", bitmap: ");
        for (i = bitmap_index; (i - bitmap_index) < nbitmap_blocks; i++) {
            Console::putch('[');
            Console::puti(i);
            Console::puts("] ");
            Console::putux(bitmap[i]);
            Console::putch(' ');
        }
        Console::putch('\n');
#endif
        nframes -= _n_frames;
        // Update bitmap
        i = bitmap_index;
        mask = 0x8000 >> offset;
#ifdef DEBUG_FRAMEPOOL
        Console::puts("Update head of sequence: [");
        Console::puti(i);
        Console::puts("]: ");
        Console::putux(bitmap[i]);
#endif
        bitmap[i] ^= mask; // update the head bit
#ifdef DEBUG_FRAMEPOOL
        Console::puts(" -> [");
        Console::puti(i);
        Console::puts("]: ");
        Console::putux(bitmap[i]);
        Console::putch('\n');
#endif
        // MARK ALL THE OTHER N-1 FRAMES AS ALLOCATED

        // WRITE THIS YOURSELF
        mask = (0x80 >> offset);
        // for loop for _n_frames
        for (unsigned long counter = 0; counter <= _n_frames - 1; counter++) {
            bitmap[i] ^= mask;
            mask >>= 1;
            if (mask == 0x00) {
                mask = 0x80;
                i++;
            }
        }
#ifdef DEBUG_FRAMEPOOL
        Console::puts("Updated bitmap: ");
        for (i = bitmap_index; (i - bitmap_index) < nbitmap_blocks; i++) {
            Console::putch('[');
            Console::puti(i);
            Console::puts("] ");
            Console::putux(bitmap[i]);
            Console::putch(' ');
        }
        Console::putch('\n');
#endif
    }

    return head_frame_no;
}

void ContFramePool::mark_inaccessible(unsigned long _base_frame_no,
                                      unsigned long _n_frames)
{
    unsigned int bitmap_index = (_base_frame_no - base_frame_no) / 8;
    unsigned int offset = (_base_frame_no - base_frame_no) % 8;
    // Update bitmap
    unsigned int i = bitmap_index;
    unsigned short mask = 0x8000 >> offset;
    bitmap[i] ^= mask; // update the head bit
    mask = 0x80 >> offset;
    while (mask > 0 && _n_frames > 0) {
        bitmap[i] ^= mask;
        mask >>= 1;
        _n_frames--;
    }
    i++;
    while (_n_frames >= 8) {
        bitmap[i] &= 0xFF00;
        _n_frames -= 8;
        i++;
    }
    mask = 0x80;
    while (_n_frames > 0) {
        bitmap[i] ^= mask;
        mask >>= 1;
        _n_frames--;
    }
    assert(mask != 0);
}

bool ContFramePool::check_and_release_frames(unsigned long _first_frame_no)
{
    unsigned long end_frame_no = base_frame_no + nframes;
    if ((_first_frame_no < base_frame_no) || (_first_frame_no >= end_frame_no)) {
        return false;
    }

    // Update bitmap
    unsigned int bitmap_index = (_first_frame_no - base_frame_no) / 8;
    unsigned int offset = (_first_frame_no - base_frame_no) % 8;

    unsigned int i = bitmap_index;
    unsigned short head_bit_mask = 0x8000 >> offset;
    unsigned short free_bit_mask = 0x80 >> offset;

    if ((bitmap[i] & head_bit_mask) != 0) {
        Console::puts("ERROR: Frame to be released is not head of sequence\n");
        return false;
    }
    bitmap[i] |= head_bit_mask; // update the head bit

    head_bit_mask = 0x8000 >> offset;
    while (((bitmap[i] & head_bit_mask) != 0) &&
        ((bitmap[i] & free_bit_mask) == 0) &&
        (free_bit_mask != 0)) {
        bitmap[i] |= free_bit_mask;
        head_bit_mask >>= 1;
        free_bit_mask >>= 1;
    }
    if (free_bit_mask != 0) {
        return true;
    }

    i++;
    while ((bitmap[i] == 0xFF00) && (i * 8 < nframes)) {
        bitmap[i] = 0xFFFF;
        i++;
    }

    if (i * 8 >= nframes) {
        return true;
    }

    head_bit_mask = 0x8000;
    free_bit_mask = 0x80;
    while (((bitmap[i] & head_bit_mask) != 0) && ((bitmap[i] & free_bit_mask) == 0)) {
        // WRITE THIS YOURSELF
        // this condition passes for allocated frames
        // simply set the entire bitmap to 11111111 11111111
        bitmap[i] = 0xFFFF;
        i++;
    }
    assert(free_bit_mask != 0);
    return true;
}

void ContFramePool::release_frames(unsigned long _first_frame_no)
{
    ContFramePool * frame_pool_ptr = list_head;

    while (frame_pool_ptr != NULL) {
        if (frame_pool_ptr->check_and_release_frames(_first_frame_no) == true) {
            return;
        }
        // GO TO THE NEXT POOL
        // WRITE THIS YOURSELF
        frame_pool_ptr = frame_pool_ptr->next_pool;
    }

    Console::puts("ERROR: Frame not released, belong to no one or not allcated\n");
    assert(0);
}

unsigned long ContFramePool::needed_info_frames(unsigned long _n_frames)
{
    return (_n_frames / (16 KB * 8) + ((_n_frames % (16 KB * 8) == 0 ? 0 : 1)));  // Check if the calculation is correct
}
