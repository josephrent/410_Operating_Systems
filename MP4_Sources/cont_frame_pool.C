/*
 File: ContFramePool.C
 
 Author:
 Date  : 
 
 */

/*--------------------------------------------------------------------------*/
/* 
 POSSIBLE IMPLEMENTATION
 -----------------------

 The class SimpleFramePool in file "simple_frame_pool.H/C" describes an
 incomplete vanilla implementation of a frame pool that allocates 
 *single* frames at a time. Because it does allocate one frame at a time, 
 it does not guarantee that a sequence of frames is allocated contiguously.
 This can cause problems.
 
 The class ContFramePool has the ability to allocate either single frames,
 or sequences of contiguous frames. This affects how we manage the
 free frames. In SimpleFramePool it is sufficient to maintain the free 
 frames.
 In ContFramePool we need to maintain free *sequences* of frames.
 
 This can be done in many ways, ranging from extensions to bitmaps to 
 free-lists of frames etc.
 
 IMPLEMENTATION:
 
 One simple way to manage sequences of free frames is to add a minor
 extension to the bitmap idea of SimpleFramePool: Instead of maintaining
 whether a frame is FREE or ALLOCATED, which requires one bit per frame, 
 we maintain whether the frame is FREE, or ALLOCATED, or HEAD-OF-SEQUENCE.
 The meaning of FREE is the same as in SimpleFramePool. 
 If a frame is marked as HEAD-OF-SEQUENCE, this means that it is allocated
 and that it is the first such frame in a sequence of frames. Allocated
 frames that are not first in a sequence are marked as ALLOCATED.
 
 NOTE: If we use this scheme to allocate only single frames, then all 
 frames are marked as either FREE or HEAD-OF-SEQUENCE.
 
 NOTE: In SimpleFramePool we needed only one bit to store the state of 
 each frame. Now we need two bits. In a first implementation you can choose
 to use one char per frame. This will allow you to check for a given status
 without having to do bit manipulations. Once you get this to work, 
 revisit the implementation and change it to using two bits. You will get 
 an efficiency penalty if you use one char (i.e., 8 bits) per frame when
 two bits do the trick.
 
 DETAILED IMPLEMENTATION:
 
 How can we use the HEAD-OF-SEQUENCE state to implement a contiguous
 allocator? Let's look a the individual functions:
 
 Constructor: Initialize all frames to FREE, except for any frames that you 
 need for the management of the frame pool, if any.
 
 get_frames(_n_frames): Traverse the "bitmap" of states and look for a 
 sequence of at least _n_frames entries that are FREE. If you find one, 
 mark the first one as HEAD-OF-SEQUENCE and the remaining _n_frames-1 as
 ALLOCATED.

 release_frames(_first_frame_no): Check whether the first frame is marked as
 HEAD-OF-SEQUENCE. If not, something went wrong. If it is, mark it as FREE.
 Traverse the subsequent frames until you reach one that is FREE or 
 HEAD-OF-SEQUENCE. Until then, mark the frames that you traverse as FREE.
 
 mark_inaccessible(_base_frame_no, _n_frames): This is no different than
 get_frames, without having to search for the free sequence. You tell the
 allocator exactly which frame to mark as HEAD-OF-SEQUENCE and how many
 frames after that to mark as ALLOCATED.
 
 needed_info_frames(_n_frames): This depends on how many bits you need 
 to store the state of each frame. If you use a char to represent the state
 of a frame, then you need one info frame for each FRAME_SIZE frames.
 
 A WORD ABOUT RELEASE_FRAMES():
 
 When we releae a frame, we only know its frame number. At the time
 of a frame's release, we don't know necessarily which pool it came
 from. Therefore, the function "release_frame" is static, i.e., 
 not associated with a particular frame pool.
 
 This problem is related to the lack of a so-called "placement delete" in
 C++. For a discussion of this see Stroustrup's FAQ:
 http://www.stroustrup.com/bs_faq2.html#placement-delete
 
 */
/*--------------------------------------------------------------------------*/


/*--------------------------------------------------------------------------*/
/* DEFINES */
/*--------------------------------------------------------------------------*/
#define KB * (0x1 << 10)
/* -- (none) -- */

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
/* -- (none) -- */

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
                             unsigned long _nframes,
                             unsigned long _info_frame_no,
                             unsigned long _n_info_frames)
{
    base_frame_no = _base_frame_no;
    nframes = _nframes;
    n_free_frames = _nframes;
    info_frame_no = _info_frame_no;
    n_info_frames = _n_info_frames;

    // If _info_frame_no is zero then we keep management info in the first
    // frame, else we use the provided frame(s) to keep management info
    if (info_frame_no == 0) {
        // Bitmap must fit in a single frame, each frame needs 2 bits
        assert(_nframes * 2 <= FRAME_SIZE * 8);
        bitmap = (unsigned short *) (base_frame_no * FRAME_SIZE);  // unsigned short * bitmap;
    } else {
        assert(_nframes * 2 <= FRAME_SIZE * 8 * _n_info_frames);
        bitmap = (unsigned short *) (info_frame_no * FRAME_SIZE);
    }

    // Number of frames must be "fill" the bitmap!
    assert ((nframes % (8 * 2)) == 0);

    // Everything ok. Proceed to mark all bits in the bitmap to be FREE
    memsetw(bitmap, 0xFFFF, _nframes / 8);
    for(unsigned int i = 0; i < _nframes / 8; i++) {
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
    }
    else {
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
    while((bitmap[i] & free_bit_mask) == 0x0) {
        i++;
    }
    bitmap_index = i;
    while((bitmap[i] & mask) == 0) {
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
