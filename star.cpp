#include "star.h"
#include <Arduino.h>
#include <ArduinoJson.h>

Star::Star(const char* name, double ra, double dec)
{
    this->name = name;
    this->ra = ra;
    this->dec = dec;
}

int compare_stars(const void* a_ptr, const void* b_ptr)
{
    Star* a = (Star *)a_ptr;
    Star* b = (Star *)b_ptr;
    if(a->distance_from_zenith < b->distance_from_zenith)
    {
        return -1;
    }
    else if(a->distance_from_zenith > b->distance_from_zenith)
    {
      return 1;
    }
    return 0;
}

void parse_json(const char* json, Star* stars, int n_stars)
{
    JsonDocument doc;//(JSON_DESERIALIZATION_BUFFER_LENGTH);

    DeserializationError error = deserializeJson(doc, json, 8501);

    if (error)
    {
        Serial.print("deserializeJson() failed: ");
        Serial.println(error.c_str());
        return;
    }

    for(int i = 0; i < n_stars; i++)
    {
        JsonObject item = doc.as<JsonArray>()[i];
        stars[i] = Star(item["Name"], item["RA"], item["Dec"]);
    }
}