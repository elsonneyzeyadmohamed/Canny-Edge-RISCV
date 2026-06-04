#include "hysteresis.h"
#include <queue>
using namespace std;

static const uint8_t STRONG = 255;
static const uint8_t WEAK   = 128;

// Our algorithm divides edges as follows into 3 types:
// STRONG (255) → definitely an edge
// WEAK (128) → maybe an edge, not sure
// NONE (0) → not an edge
/*
1 Find all STRONG pixels -> put them in a queue
2 For each STRONG pixel, check its 8 neighbours
3 If neighbour is WEAK -> promote to STRONG, add to queue
4 If neighbour is NONE or already STRONG -> skip
5 After BFS -> any pixel still WEAK was unreachable -> set to 0
*/
// Note we jump 1 by not 8 by 8 to avoid unchecking pixels in between


void hysteresis(uint8_t* edges, int width, int height)
{
    queue<int> q;
     // find strong pixels
    // ── Phase 1: seed the queue with every STRONG pixel ──────────────────────
    for (int i = 0; i < width * height; i++)
        if (edges[i] == STRONG) q.push(i);

    // ── Phase 2: BFS – promote connected WEAK pixels ─────────────────────────
    // 8-connectivity: all directions including diagonals
    // finding 8 elements matrix, dr and dc are rows and columns number each element (dr[d], dc[d])
    const int dr[8] = {-1, -1,  0,  1, 1,  1,  0, -1};
    const int dc[8] = { 0,  1,  1,  1, 0, -1, -1, -1};

    while (!q.empty()) {
// read first element in the queue to check it then remove it to avoid using it again
        int idx = q.front(); q.pop();
//Convert flat index back to 2D coordinates. Example: pixel 515 in a 512-wide image → row = 515/512 = 1, col = 515%512 = 3.
        int row = idx / width;
        int col = idx % width;

// Compute the neighbour's coordinates by adding the offset.

        for (int d = 0; d < 8; d++) {
            int nr = row + dr[d];
            int nc = col + dc[d];

            // Skip out-of-bounds neighbours
            if (nr < 0 || nr >= height || nc < 0 || nc >= width) continue;

            int nidx = nr * width + nc;

            // Only promote WEAK neighbours — STRONG already processed, NONE ignored
            if (edges[nidx] == WEAK) {
                edges[nidx] = STRONG;   // promote
                q.push(nidx);           // continue BFS from this new strong pixel
            }
        }
    }

    // ── Phase 3: cleanup – any remaining WEAK pixel is isolated noise ─────────
    for (int i = 0; i < width * height; i++)
        if (edges[i] == WEAK) edges[i] = 0;
}
