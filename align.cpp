#include "align.h"
#include "AstroCalcs.h"
#include "star.h"


void sort_stars(AstroCalcs calcs, Star* stars, int n_stars)
{
    for(int i = 0; i < n_stars; i++)
    {
        calcs.setRADEC(stars[i].ra, stars[i].dec);
        stars[i].distance_from_zenith = 90 - calcs.curr_pos.alt;
        stars[i].ra = calcs.curr_pos.ra;
        stars[i].dec = calcs.curr_pos.dec;
    }
    qsort(stars, n_stars, sizeof(Star), compare_stars);
}