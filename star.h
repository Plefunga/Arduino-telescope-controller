#ifndef STAR_H
#define STAR_H 1

#define JSON_DESERIALIZATION_BUFFER_LENGTH 8192

#include <Arduino.h>

class Star {
    public:
    Star(const char* name, double ra, double dec);
    Star(){}

    double distance_from_zenith;
    String name;
    double ra;
    double dec;
    
};

/**
 * Parse star data from json string
 * @param json the json string
 * @param stars an array of stars
 * @param n_stars how many stars in the array
 * @returns acts in place on data
 */
void parse_json(const char* json, Star* stars, int n_stars);

/**
 * Compares the distance of 2 stars from zenith
 * @param a the first star
 * @param b the second star
 * @returns -1 if a distance from zenith < b distance from zenith, 0 if equal, and 1 if a > b
 */
int compare_stars(const void* a, const void* b);
#endif