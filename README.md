lua-snowflake
=============

An implementation of [Snowflake](https://blog.twitter.com/2010/announcing-snowflake) for Lua. Snowflake is an algorithm
which supports ordered, distributed id generation. 

Installation
------------

    $ luarocks install snowflake
     
Usage
-----

    local sf = require "snowflake"
    sf.init(0x01, 0x01)
    local uuid = sf.next_id()

`uuid` will be a 64-bit number represented as a string that is structured as follows:

    6  6         5         4         3         2         1         
    3210987654321098765432109876543210987654321098765432109876543210
    
    tttttttttttttttttttttttttttttttttttttttttdddddnnnnnsssssssssssss

where default

* `s` is a 12-bit integer that increments when next_id() is called multiple times for the same millisecond
* `n` is a 5-bit integer representing the node within a given data center
* `d` is a 5-bit integer representing a unique data center or group of servers
* `t` is a 42-bit integer representing the current timestamp in milliseconds
    * the number of milliseconds to have elapsed since 1413817200000 or 2014-10-20T15:00:00.000Z

`sf.init(datacenter_id, node_id, snowflake_epoc, node_id_bits, datacenter_id_bits, sequence_bits, delta_offset)` is used to initialize snowflake and set values for ddddd and nnnnn as follows:

* `datacenter_id` is an integer `d`, where 0 ≤ d < (1 << datacenter_id_bits) and specifies the `ddddd` portion of the id.
* `node_id` is an integer `n`, where 0 ≤ n < (1 << node_id_bits) and specifies the `nnnnn` portion of the id.
* `snowflake_epoc` the start timestamp is expressed in milliseconds, the part beyond delta_offset will be ignored.
* `node_id_bits` is an integer representing the number of bits occupied by a node in a given data center.
* `datacenter_id_bits` is an integer representing the number of bits occupied by a unique data center or a group of servers
* `sequence_bits` is an integer, the number of bits occupied by the number of calls in the same millisecond.
* `delta_offset` timestamp delta offset (unit: milliseconds) [`delta_offset=1` 1 millisecond, `delta_offset=10` 10 milliseconds, `delta_offset=100` 100 milliseconds, `delta_offset=1000` 1 second`]
