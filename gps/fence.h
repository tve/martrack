// Copyright (c) 2018 by Thorsten von Eicken
//
// Geo-fences for GPS

#include <math.h>

//  Determine whether a point is in a polygon.
//  Adapted from http://alienryderflex.com/polygon/
//
//  Parameters:
//  int    vertices  =  how many corners the polygon has
//  float  polyX[]   =  horizontal coordinates of corners in degrees
//  float  polyY[]   =  vertical coordinates of corners in degrees
//  float  x, y      =  point to be tested
//
//  The function will return YES if the point x,y is inside the polygon, or
//  NO if it is not.  If the point is exactly on the edge of the polygon,
//  then the function may return YES or NO.
//
//  Note that division by zero is avoided because the division is protected
//  by the "if" clause which surrounds it.
bool pointInPolygon(int vertices, float polyX[], float polyY[], float x, float y) {

    int   j = vertices-1;
    bool  oddNodes = false;

    for (int i=0; i<vertices; i++) {
        if (((polyY[i] < y && polyY[j] >= y) || (polyY[j] < y && polyY[i] >= y))
        &&  (polyX[i] <= x || polyX[j] <= x)) {
            oddNodes ^= polyX[i]+(y-polyY[i])/(polyY[j]-polyY[i])*(polyX[j]-polyX[i]) < x;
        }
        j = i;
    }

    return oddNodes;
}

// geoDistance returns the distance in meters between two positions specified
// using fixed-point minutes*1000 lat/lon.
// The great-circle distance computation uses the Vincenty formula from
// https://en.wikipedia.org/wiki/Great-circle_distance
// The earth is approximated as a sphere of radius 6371m.
uint32_t geoDistance(uint32_t lat1, uint32_t lon1, uint32_t lat2, uint32_t lon2) {
    constexpr double m2r = 3.14159265359 / 180 / 60 / 1000; // minutes*1E3 to radians
    // for longitudes we only the delta between the two
    double rlon1 = (double)lon1 * m2r;
    double rlon2 = (double)lon2 * m2r;
    double delta_lon = rlon1 - rlon2;
    double sin_dlon = sin(delta_lon);
    double cos_dlon = cos(delta_lon);
    // for latitudes we need sin/cos for each
    double rlat1 = (double)lat1 * m2r;
    double sin_lat1 = sin(rlat1);
    double cos_lat1 = cos(rlat1);
    double rlat2 = (double)lat2 * m2r;
    double sin_lat2 = sin(rlat2);
    double cos_lat2 = cos(rlat2);
    // numerator of the Vincenty formula
    double num1 = cos_lat2 * sin_dlon;
    double num2 = cos_lat1 * sin_lat2 - sin_lat1 * cos_lat2 * cos_dlon;
    double num = sqrt(num1*num1 + num2*num2);
    double denom = sin_lat1 * sin_lat2 + cos_lat1 * cos_lat2 * cos_dlon;
    return uint32_t(6371 * atan2(num, denom) + 0.5);
}

#if 0
// https://www.igismap.com/formula-to-find-bearing-or-heading-angle-between-two-points-latitude-longitude
double courseTo(double lat1, double long1, double lat2, double long2)
{
  // returns course in degrees (North=0, West=270) from position 1 to position 2,
  // both specified as signed decimal-degrees latitude and longitude.
  // Because Earth is no exact sphere, calculated course may be off by a tiny fraction.
  // Courtesy of Maarten Lamers
  double dlon = radians(long2-long1);
  lat1 = radians(lat1);
  lat2 = radians(lat2);
  double a1 = sin(dlon) * cos(lat2);
  double a2 = sin(lat1) * cos(lat2) * cos(dlon);
  a2 = cos(lat1) * sin(lat2) - a2;
  a2 = atan2(a1, a2);
  if (a2 < 0.0)
  {
    a2 += TWO_PI;
  }
  return degrees(a2);
}

const char *TinyGPSPlus::cardinal(double course)
{
  static const char* directions[] = {"N", "NNE", "NE", "ENE", "E", "ESE", "SE", "SSE", "S", "SSW", "SW", "WSW", "W", "WNW", "NW", "NNW"};
  int direction = (int)((course + 11.25f) / 22.5f);
  return directions[direction % 16];
}
#endif
