/*
 * Codec for Air Quality Device on LNS (TTN, Helium, Chirpstack)
 *  Author: Didier DONSEZ (Université Grenoble Alpes)
 */

// TODO TTN v3

/*

Usage:

var payload = Buffer.from("AHAJAScsEgAAAAAAAAAAAAAAALQAOAACAAIAAAA=","base64");
console.log(Decode(101,payload,null));

{
  temperature: 24.16,
  pressure: 998.5,
  humidity: 46.52,
  pm1_0Standard: 0,
  pm2_5Standard: 0,
  pm10Standard: 0,
  pm1_0Atmospheric: 0,
  pm2_5Atmospheric: 0,
  pm10Atmospheric: 0,
  particuleGT0_3: 180,
  particuleGT0_5: 56,
  particuleGT1_0: 2,
  particuleGT2_5: 0,
  particuleGT10: 0
}

payload = Buffer.from("03","hex");
console.log(Decode(101,payload,null));

{ bmx280_error: true, pms7003_error: true }

*/


function readUInt16LE (buf, offset) {
    offset = offset >>> 0;
    return buf[offset] | (buf[offset + 1] << 8);
}

function readInt16LE (buf, offset) {
    offset = offset >>> 0;
    var val = buf[offset] | (buf[offset + 1] << 8);
    return (val & 0x8000) ? val | 0xFFFF0000 : val;
}


// TODO: Decode App Sync Clock message
function Decode202(bytes, variables, object) {
    return object;
}


// TODO: Decode Data message
function DecodeData(bytes, variables, o) {

    var size = bytes.length;

    if (size >= 1) {

        var flags = bytes[0];
        var i = 1;

        if(((flags & 0x01) === 0) && (size >= i + 6)) {
            o['temperature'] = readInt16LE(bytes, i) / 100.0; // in °C
            i += 2;
            o['pressure'] = readUInt16LE(bytes, i) / 10.0; // in hPa
            i += 2;
            o['humidity'] = readUInt16LE(bytes, i) / 100.0; // in %
            i += 2;
        } else {
            o['bmx280_error'] = true;
        }

        if(((flags & 0x01) === 0) && (size >= i + 22)) {
            o['pm1_0Standard'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;
            o['pm2_5Standard'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;
            o['pm10Standard'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;
            
            o['pm1_0Atmospheric'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;
            o['pm2_5Atmospheric'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;
            o['pm10Atmospheric'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;

            o['particuleGT0_3'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;
            o['particuleGT0_5'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;
            o['particuleGT1_0'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;
            o['particuleGT2_5'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;
            o['particuleGT2_5'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;
            o['particuleGT10'] = readUInt16LE(bytes, i); // in ug/m3
            i += 2;            
        } else {
            o['pms7003_error'] = true;
        }
        return o;
    } else {
        return { _errors: ["data too short"] };
    }    
}


// Chirpstack
// Decode decodes an array of bytes into an object.
//  - fPort contains the LoRaWAN fPort number
//  - bytes is an array of bytes, e.g. [225, 230, 255, 0]
//  - variables contains the device variables e.g. {"calibration": "3.5"} (both the key / value are of type string)
// The function must return an object, e.g. {"temperature": 22.5}
function Decode(fPort, bytes, variables) {

    var DATA_PORT = 101; // const
    var APPSYNCCLOCK_PORT = 202; // const

    var o = {_tags:variables}; // tags can be used in InfluxDB / Grafana to filter data

    if (fPort === DATA_PORT) {
        return DecodeData(bytes, variables, o);
    } if(fPort === APPSYNCCLOCK_PORT) {
        return Decode202(bytes, variables, o)
    } else {
        o._errors = ["unknown port " + fPort];
        return o;
    }
}

// For Helium and TTNv2
function Decoder(bytes, port) {
    return Decode(port, bytes);
}

// TODO LoRaWAN Payload Codec API https://resources.lora-alliance.org/technical-specifications/ts013-1-0-0-payload-codec-api
function decodeUplink(input) {
    var output = {};
    var variables = {recvTime: input.recvTime};
    var decoded = Decode(input.fPort, input.bytes, variables);
    if(decoded._errors) {
        output.errors = decoded._errors;  // Mandatory when failed
    } else {
        output.data = Decode(input.fPort, input.bytes, variables); // Mandatory when successful.
    }
    if(output._warnings) {
        output.warnings = decoded._warnings; // Optional
    }
    return output;
}
