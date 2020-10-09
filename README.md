# STARELite
STARELite is the SQLite STARE extensions. It allows conversions of geospatial objects to their STARE representation as well as to perform geospatial relation tests.

![Example 1](figures/STARELite_icon.png)

## Build

STARELite depends on libsqlite, libspatialite and libSTARE

    c++ -g -fPIC -L/usr/local/lib/ -L/usr/lib/x86_64-linux-gnu/ -I/usr/local/include -I/usr/include/spatialite/ -std=c++11 -shared STARELite.cpp -o STARELite.so -lspatialite -lSTARE
    

## Load extension

    SELECT load_extension("./STARELite");
    
equivalent to 

    .load ./STARELite 

in the sqlite3 shell
    
## Use  

### STARE conversions
From longitude, latitude and level

    SELECT stare_from_lonlat(5, 3.1, 2);
    SELECT name, stare_from_lonlat(longitude, latitude, 15) FROM cities;

#### From spatialite WKB blob (point): 
    
    SELECT name, stare_from_point(geometry, 15) FROM cities;

#### From spatilite WKB blob (polygon)
    
    ALTER TABLE countries ADD column stare blob;
    UPDATE countries 
    SET stare=stare_from_polygon(geometry, 5);
    
#### Conversion to human readable string:

    SELECT name, decode_stareblob(stare) 
    FROM countries

Conversion from human readable string

    SELECT decode_stareblob(encode_stareblob('[3666130979403333634, 3666130979403333634]'));

#### Stare intersects 

    SELECT stare_intersects(3666130979403333634, 3666130979403333634);

    SELECT name, stare_intersects(2667981979956219503, stare) 
    FROM countries;
    
#### STARE join

    SELECT countries.name, countries.name 
    FROM countries, cities
    WHERE stare_intersects(countries.stare, stare_from_point(cities.geom, 15));

or

    SELECT countries.name, cities.name 
    FROM cities
    LEFT JOIN countries
    ON stare_intersects(countries.stare, stare_from_point(cities.geom, 15));




