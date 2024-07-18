#ifndef ALIGN_H
#define ALIGN_H 1

#include "AstroCalcs.h"
#include "star.h"

/**
 * Sorts stars by best for polar alignment (i.e. closest to zenith)
 * @param calcs an astrocalcs object (update the time before running this funciton)
 * @param stars a pointer to the list of stars
 * @param n_stars how many stars there are in the buffer
 * @returns acts in place on the data
 */
void sort_stars(AstroCalcs calcs, Star* stars, int n_stars);

#endif