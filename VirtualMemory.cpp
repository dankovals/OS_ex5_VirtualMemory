#include <cstdio>
#include <cmath>
#include <algorithm>
#include "VirtualMemory.h"
#include "PhysicalMemory.h"


void clearTable(uint64_t frameIndex) {
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMwrite(frameIndex * PAGE_SIZE + i, 0);
    }
}

/*
 * Initialize the virtual memory
 */
void VMinitialize() {
    clearTable(0);
}


/**
 * Remove the link from the parent of the given frameIndx to it.
 */
void removeFromParent(int depth, uint64_t frameIndx, uint64_t value) //todo - hila did.
{
    if (depth == TABLES_DEPTH) {
        // this condition makes sure that we don't accidentally remove a link from frame that holds a page.
        return;
    }
    word_t curWord;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMread(frameIndx * PAGE_SIZE + i, &curWord);
        if ((uint64_t)curWord == value) {
            PMwrite(frameIndx * PAGE_SIZE + i, 0);
            return;
        }

        if (curWord != 0) {  // we need to call to the son
            removeFromParent(depth + 1, curWord, value);
        }
    }
    return;
}

/**
 * Looking for the frame that holds the page with the maximal cyclical distance from pageSwappedIn,
 * during traversal on the tree.
 * @param depth - the initial depth the recursion starts with.
 * @param maxFrameIndex - the maximal frame index which is in use.
 * @param max_cyc_frame - the index of the frame that holds the page with the maximal cyclical distance
 * @param VM_max_addr - the index of the page with the maximal cyclical distance
 * @param cur_root_frame - the index of the current frame in the recursion (the current root, we iterating on its children).
 * @param pageSwappedIn - the page index of the page we want to insert.
 * @param VM_page_index - the page index, initialized to 0 and get bigger in each iteration,
 *                        so when we get to the leaf, it contains the page index of the page in the leaf.
 */
void getMaxCycFrame(int depth, uint64_t &max_cyc_val, uint64_t &max_cyc_frame, uint64_t &VM_max_addr,
                    uint64_t cur_root_frame, uint64_t pageSwappedIn, uint64_t VM_page_index){
    if (depth == TABLES_DEPTH) {
        // calculate the cyclical distance from pageSwappedIn
        uint64_t distance = (uint64_t) abs((int) pageSwappedIn - (int) VM_page_index);
        distance = std::min(distance, (uint64_t) NUM_PAGES - distance);
        if (distance > max_cyc_val)
        {
            max_cyc_val = distance;
            max_cyc_frame = cur_root_frame;
            VM_max_addr = VM_page_index;
        }
        return;
    }
    word_t curWord;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMread(cur_root_frame * PAGE_SIZE + i, &curWord);
        if (curWord != 0){  // we need to call to the son
            getMaxCycFrame(depth+1, max_cyc_val, max_cyc_frame, VM_max_addr, curWord, pageSwappedIn,
                           VM_page_index + i * (uint64_t)pow(PAGE_SIZE,(TABLES_DEPTH-depth-1)));
        }
    }
    return;
}

/**
 * Evict a page from some frame
 * For each page, calculate the cyclical distance from page_swapped_in.
 */
void evict(word_t *newAddr, uint64_t pageSwappedIn) {
    // 1. finds pages in the RAM:
    // 2. find the page index (the reminder after taking the offset)

    uint64_t max_cyc_val = 0, max_cyc_frame = 0, VM_max_page;
    getMaxCycFrame(0, max_cyc_val, max_cyc_frame, VM_max_page, 0, pageSwappedIn, 0);
    PMevict(max_cyc_frame, VM_max_page);
    removeFromParent(0,0,max_cyc_frame); // remove the link to the this frame from its parent table.
    *newAddr = (word_t) max_cyc_frame;
    return;
}

/**
 * Make sure that the frame we evict doesn't one of the frames we need
 */
bool inParentRoute(word_t (&routeFrames)[TABLES_DEPTH], uint64_t indx) {
    for (int i = 0; i < TABLES_DEPTH; ++i) {
        if ((uint64_t)routeFrames[i] ==indx)
            return true;
    }
    return false;
}

/**
 * Looking for an empty frame during traversal on the tree.
 * @param depth - the initial depth the recursion starts with.
 * @param frameIndx - the index of the current frame in the recursion (the current root, we iterating on its children).
 * @param isNotEmpty - indicates if the current frame is empty or not.
 * @param newFrame - if found unused frame it will hold its index, otherwise it will be 0.
 */
void getEmptyFrame(int depth, uint64_t frameIndx, bool isNotEmpty,
                   word_t (&routeFrames)[TABLES_DEPTH], uint64_t &newFrame)
{
    if (!isNotEmpty){ // empty table
        if (!inParentRoute(routeFrames, frameIndx)){
            // the frame containing an empty table - where all rows are 0, and it's not part of the route
            newFrame = frameIndx;
            return;
        }
        else
            return;
    }

    if (depth == TABLES_DEPTH)
        // this condition makes sure that we don't consider accidentally empty page as empty frame.
        return;

    isNotEmpty = false;
    word_t curWord;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMread(frameIndx * PAGE_SIZE + i, &curWord);
        if (curWord != 0){
            // we need to call to the son
            isNotEmpty = true;
            getEmptyFrame(depth + 1, curWord, isNotEmpty, routeFrames, newFrame);
        }
        if((i + 1 == PAGE_SIZE) && (!isNotEmpty)){
            getEmptyFrame(depth + 1, frameIndx, isNotEmpty, routeFrames, newFrame);
        }
    }
    return;
}

/**
 * Looking for unused frame during traversal on the tree.
 * @param depth - the initial depth the recursion starts with.
 * @param frameIndx - the index of the current frame in the recursion (the current root).
 * @param maxFrameIndex - the maximal frame index which is in use.
 * @param newFrame - if found unused frame it will hold its index, otherwise it will be 0.
 */
void getUnusedFrame(int depth, uint64_t frameIndx, word_t &maxFrameIndex, uint64_t &newFrame)
{
    if (depth == TABLES_DEPTH)
        // This condition makes sure that we don't calculate
        // the maxFrameIndex in a leaf (because it contains page).
        return;

    word_t curWord;
    for (uint64_t i = 0; i < PAGE_SIZE; ++i) {
        PMread(frameIndx * PAGE_SIZE + i, &curWord);
        if (curWord != 0)
        {  // we need to call to the son
            if (curWord > maxFrameIndex) {
                maxFrameIndex = curWord;
            }
            getUnusedFrame(depth + 1, curWord, maxFrameIndex, newFrame);
        }
    }
    return;
}

/**
 * This function insert into new_addr the fit address in the phisycal memory.
 * Finds an unused frame or evict a page from some frame new_addr.
 * It also check that we not evict the frames that we use in the current operation.
 */
void findPhAddr(word_t *newAddr, uint64_t pageIndex, word_t (&routeFrames)[TABLES_DEPTH]) {
    uint64_t newFrame = 0, frameIndx = 0;
    bool isNotEmpty = true;
    word_t maxFrameIndex = 0; // the maximal frame index which is in use.

    getEmptyFrame(0, frameIndx, isNotEmpty, routeFrames, newFrame);
    if (newFrame != 0)  { // it means that we found frame containing empty table.
        *newAddr = (word_t) newFrame;
        removeFromParent(0, 0, newFrame);  //remove the reference to newAddr from its parent.
        return;
    }

    getUnusedFrame(0, frameIndx, maxFrameIndex, newFrame);
    if (maxFrameIndex + 1 < NUM_FRAMES) {
        // we will get here if there is unused frame in the physical memory.
        *newAddr = maxFrameIndex + 1;
        return;
    }

    // If we got here, we need to evict a page from some frame
    evict(newAddr, pageIndex);
    return;
}

/**
 * Translate a virtual address into a physical address.
 * @param valueToWrite - value to write into the virtual address.
 *                       Will be other than -1 only if caller is VMwrite.
 * @param valueToFill - value to fill with the word in the given virtual address.
 *                      Will be other than nullptr only if caller is VMread.
 * @param caller - 0 if caller is VMread, otherwise 1.
 */
void translateVAddress(uint64_t virtualAddress, word_t valueToWrite, word_t* valueToFill, int caller){
    word_t routeFrames[TABLES_DEPTH] = {0};  // contains the current route from the route to the leaf.
    word_t cur_addr = 0;
    word_t new_addr; // frame index
    uint64_t curIdx;  // 4 bits of the current position
    uint64_t pageIndex = virtualAddress >> (uint64_t)OFFSET_WIDTH;
    uint64_t leftNum = virtualAddress; // all the left bits, each loop 4 bits smaller
    for (int level = 0; level < TABLES_DEPTH; ++level){
        curIdx = leftNum >> (uint64_t )OFFSET_WIDTH * (TABLES_DEPTH-level);
        leftNum %= (uint64_t )pow(2, OFFSET_WIDTH * ((uint64_t )TABLES_DEPTH-level));

        PMread(cur_addr * PAGE_SIZE + curIdx, &new_addr);
        if (new_addr == 0) {
            //we know that this is not a real frame index and we have reached a page fault.
            // Now we have to swap the page back in and add it to the tree, creating
            // tables above it and evicting other pages if necessary.
            findPhAddr(&new_addr, pageIndex, routeFrames);
            clearTable((uint64_t)new_addr); // Write 0 in all of its contents
            PMwrite(cur_addr * PAGE_SIZE + curIdx, new_addr);  // map our new table to frame new_addr
        }
        cur_addr = new_addr;
        routeFrames[level] = new_addr;

        if (level + 1 == TABLES_DEPTH){  // we are in the actual page (leaf), ready for writing/reading
            if (caller == 1){
                PMrestore((uint64_t)cur_addr, (uint64_t)pageIndex);
                PMwrite(cur_addr * PAGE_SIZE + leftNum, valueToWrite);  // here leftnum is the offset
            } else { // caller is reader
                PMrestore((uint64_t)cur_addr, (uint64_t)pageIndex);
                PMread(cur_addr * PAGE_SIZE + leftNum, valueToFill);  // here leftnum is the offset
            }
        }
    }
}

/* writes a word to the given virtual address
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMwrite(uint64_t virtualAddress, word_t value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;  // From the forum
    }
    translateVAddress(virtualAddress, value, nullptr, 1);
    return 1;
}

/* reads a word from the given virtual address
 * and puts its content in *value.
 *
 * returns 1 on success.
 * returns 0 on failure (if the address cannot be mapped to a physical
 * address for any reason)
 */
int VMread(uint64_t virtualAddress, word_t* value) {
    if (virtualAddress >= VIRTUAL_MEMORY_SIZE) {
        return 0;  // From the forum
    }
    translateVAddress(virtualAddress, -1, value, 0);
    return 1;
}


