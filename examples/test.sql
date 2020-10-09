SELECT load_extension("./STARELite");
SELECT load_extension("mod_spatialite");


SELECT stare_from_lonlat(5, 3.1, 2);
SELECT name, stare_from_lonlat(longitude, latitude, 15) FROM cities;
SELECT name, stare_from_point(geometry, 15) FROM cities;


ALTER TABLE countries ADD column stare blob;
UPDATE countries 
SET stare=stare_from_polygon(geometry, 5) 
WHERE continent = 'South America';

SELECT name, decode_stareblob(stare) 
FROM countries
WHERE continent = 'South America';

SELECT decode_stareblob(encode_stareblob('[3666130979403333634, 3666130979403333634]'));

SELECT stare_intersects(3666130979403333634, 3666130979403333634);
SELECT stare_intersects(2867415364672350639, 2723774768829278543);

SELECT name, stare_intersects(2667981979956219503, stare) 
FROM countries
WHERE continent = 'South America';;

SELECT countries.name, countries.name 
FROM countries, cities
WHERE stare_intersects(countries.stare, stare_from_point(cities.geom, 15));


SELECT countries.name, cities.name 
FROM cities
LEFT JOIN countries
ON stare_intersects(countries.stare, stare_from_point(cities.geom, 15));

