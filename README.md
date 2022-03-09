# STARELite
STARELite is the SQLite STARE extensions. It allows conversions of geospatial objects to their STARE representation as well as to perform geospatial relation tests.

![Example 1](figures/STARELite_icon.png)

## Build

STARELite depends on libsqlite, libspatialite and libSTARE

```bash
c++ -g -fPIC -L/usr/local/lib/ 
    -L/usr/lib/x86_64-linux-gnu/ 
    -I/usr/local/include 
    -I/usr/include/spatialite/ 
    -std=c++11 
    -shared 
    STARELite.cpp -o STARELite.so 
    -lspatialite -lSTARE
```
    

## Load extension

```SQL
SELECT load_extension("./STARELite");
```
    
equivalent to 
```SQL
.load ./STARELite 
```

in the sqlite3 shell

For some functions, the spatialite extension needs to be loaded as well:
```sql
SELECT load_extension("mod_spatialite");
```
    
## Useage

### STARE conversions
From longitude, latitude, and level 

```SQL
    SELECT stare_from_lonlat(5, 3.1, 2);
    SELECT name, stare_from_lonlat(longitude, latitude, 15) FROM cities;
```

#### From spatialite WKB blob (point): 
    
```SQL
    SELECT name, stare_from_point(geometry, 15) FROM cities;
```

#### From spatilite WKB blob (polygon)

```SQL
    ALTER TABLE countries ADD column stare blob;
    UPDATE countries 
    SET stare=stare_from_polygon(geometry, 5);
```
    
#### Conversion between human readable strings and STAREBlobs

```SQL
    SELECT name, decode_stareblob(stare) 
    FROM countries
```

Conversion from human readable string

```SQL
    SELECT encode_stareblob('[3666130979403333634, 3666130979403333634]'));
```

#### Stare intersects 

```SQL
    SELECT stare_intersects(3666130979403333634, 3666130979403333634);

    SELECT name, stare_intersects(2667981979956219503, stare) 
    FROM countries;
```
    
#### STARE join

```SQL
    SELECT countries.name, countries.name 
    FROM countries, cities
    WHERE stare_intersects(countries.stare, stare_from_point(cities.geom, 15));
```

or

```SQL
    SELECT countries.name, cities.name 
    FROM cities
    LEFT JOIN countries
    ON stare_intersects(countries.stare, stare_from_point(cities.geom, 15));
```


### Loading Data From DataFrame
A STAREDataFrame stores collections of SIDs representing a cover as 1D numpy arrays.
To inject those into SQLite, we have to serialize the arrays and then encode them to STARE blobs using ```encode_starebolob()```. 
For now, we serialize the arrays to strings. E.g.:

```python
sdf['sids_serialized'] = sdf.apply(lambda row : str(list(row['sids'])), axis = 1)
```

Then we can do: 

```python
sdf.to_file('data.gpkg', driver='GPKG')
```

Finally, we can bootstrap the STAREBlobs with

```sql
SELECT load_extension("./STARELite");
SELECT encode_stareblob(sids_serialized) FROM featuredb;
```


#### Bootstrap a database:
```sql
ALTER TABLE featuredb ADD COLUMN sids BLOB;
UPDATE featuredb set sids = encode_stareblob(sids_s);
```

```sql
ALTER TABLE featuredb ADD COLUMN precip BLOB;
UPDATE featuredb set precip = encode_float_blob(precip_s);
```


```sql
ALTER TABLE featuredb ADD COLUMN area BLOB;
UPDATE featuredb set area = encode_float_blob(areas_s);
```


```sql
ALTER TABLE featuredb ADD COLUMN precip BLOB;
UPDATE featuredb set precip = encode_float_blob(precip_s);
```
