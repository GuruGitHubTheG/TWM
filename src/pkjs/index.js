// ---------- PebbleKit JS – Configuration + Custom Photo ----------

var BASE_CONFIG_URL = 'https://gurugithubtheg.github.io/TWM/';
var configPageUrl = null;
var configTimeout = null;
var waitingForConfig = false;

// Message keys (match main.c)
var KEY_REQUEST_CONFIG = 0;
var KEY_CONFIG_DATA = 1;
var KEY_INVERTED = 2;
var KEY_WALLPAPER = 3;
var KEY_CLOCK_MODE = 4;
var KEY_LEADING_ZEROS = 5;
var KEY_SHOW_AMPM = 6;
var KEY_UI_COLOR = 7;
var KEY_FLICK_WINDOW = 8;
var KEY_HOURLY_VIBRATION = 21;
var KEY_BT_DISCONNECT_VIBRATION = 22;
var KEY_HOURLY_CHIME = 23;
var KEY_IMAGE_REQUEST = 10;
var KEY_IMAGE_BEGIN = 11;
var KEY_IMAGE_WIDTH = 12;
var KEY_IMAGE_HEIGHT = 13;
var KEY_IMAGE_LENGTH = 14;
var KEY_IMAGE_CHECKSUM = 15;
var KEY_IMAGE_CHUNK = 16;
var KEY_IMAGE_OFFSET = 17;
var KEY_IMAGE_END = 18;
var KEY_IMAGE_DESIRED_CHECKSUM = 19;
var KEY_IMAGE_PALETTE = 20;

var WALLPAPER_CUSTOM = 8;
var CUSTOM_CHUNK_SIZE = 900;
var MAX_SEND_ATTEMPTS = 3;
var MAX_CONFIG_TRANSACTION_ATTEMPTS = 3;

var CUSTOM_PHOTO_STORAGE_PREFIX = 'twm.customphoto.v1.';

var volatileImages = {};
var sendQueue = [];
var sending = false;
var transfer = null;
var pendingTransfer = null;
var photoTransferGeneration = 0;

var watchVersionFromConfig = null;

// ---------- LZString Decompression (compressToEncodedURIComponent) ----------
var LZString = (function() {
    var f = String.fromCharCode;
    var keyStrUriSafe = "ABCDEFGHIJKLMNOPQRSTUVWXYZabcdefghijklmnopqrstuvwxyz0123456789+-$";
    var baseReverseDic = {};

    function getBaseValue(alphabet, character) {
        if (!baseReverseDic[alphabet]) {
            baseReverseDic[alphabet] = {};
            for (var i = 0; i < alphabet.length; i++) {
                baseReverseDic[alphabet][alphabet.charAt(i)] = i;
            }
        }
        return baseReverseDic[alphabet][character];
    }

    function decompressFromEncodedURIComponent(input) {
        if (input == null) return "";
        if (input == "") return null;
        input = input.replace(/ /g, "+");
        return _decompress(input.length, 32, function(index) {
            return getBaseValue(keyStrUriSafe, input.charAt(index));
        });
    }

    function _decompress(length, resetValue, getNextValue) {
        var dictionary = [],
            next,
            enlargeIn = 4,
            dictSize = 4,
            numBits = 3,
            entry = "",
            result = [],
            i,
            w,
            bits, resb, maxpower, power,
            c,
            data = { val: getNextValue(0), position: resetValue, index: 1 };

        for (i = 0; i < 3; i += 1) {
            dictionary[i] = i;
        }

        bits = 0;
        maxpower = Math.pow(2, 2);
        power = 1;
        while (power != maxpower) {
            resb = data.val & data.position;
            data.position >>= 1;
            if (data.position == 0) {
                data.position = resetValue;
                data.val = getNextValue(data.index++);
            }
            bits |= (resb > 0 ? 1 : 0) * power;
            power <<= 1;
        }

        switch (next = bits) {
            case 0:
                bits = 0;
                maxpower = Math.pow(2, 8);
                power = 1;
                while (power != maxpower) {
                    resb = data.val & data.position;
                    data.position >>= 1;
                    if (data.position == 0) {
                        data.position = resetValue;
                        data.val = getNextValue(data.index++);
                    }
                    bits |= (resb > 0 ? 1 : 0) * power;
                    power <<= 1;
                }
                c = f(bits);
                break;
            case 1:
                bits = 0;
                maxpower = Math.pow(2, 16);
                power = 1;
                while (power != maxpower) {
                    resb = data.val & data.position;
                    data.position >>= 1;
                    if (data.position == 0) {
                        data.position = resetValue;
                        data.val = getNextValue(data.index++);
                    }
                    bits |= (resb > 0 ? 1 : 0) * power;
                    power <<= 1;
                }
                c = f(bits);
                break;
            case 2:
                return "";
        }
        dictionary[3] = c;
        w = c;
        result.push(c);
        while (true) {
            if (data.index > length) {
                return "";
            }

            bits = 0;
            maxpower = Math.pow(2, numBits);
            power = 1;
            while (power != maxpower) {
                resb = data.val & data.position;
                data.position >>= 1;
                if (data.position == 0) {
                    data.position = resetValue;
                    data.val = getNextValue(data.index++);
                }
                bits |= (resb > 0 ? 1 : 0) * power;
                power <<= 1;
            }

            switch (c = bits) {
                case 0:
                    bits = 0;
                    maxpower = Math.pow(2, 8);
                    power = 1;
                    while (power != maxpower) {
                        resb = data.val & data.position;
                        data.position >>= 1;
                        if (data.position == 0) {
                            data.position = resetValue;
                            data.val = getNextValue(data.index++);
                        }
                        bits |= (resb > 0 ? 1 : 0) * power;
                        power <<= 1;
                    }

                    dictionary[dictSize++] = f(bits);
                    c = dictSize - 1;
                    enlargeIn--;
                    break;
                case 1:
                    bits = 0;
                    maxpower = Math.pow(2, 16);
                    power = 1;
                    while (power != maxpower) {
                        resb = data.val & data.position;
                        data.position >>= 1;
                        if (data.position == 0) {
                            data.position = resetValue;
                            data.val = getNextValue(data.index++);
                        }
                        bits |= (resb > 0 ? 1 : 0) * power;
                        power <<= 1;
                    }
                    dictionary[dictSize++] = f(bits);
                    c = dictSize - 1;
                    enlargeIn--;
                    break;
                case 2:
                    return result.join('');
            }

            if (enlargeIn == 0) {
                enlargeIn = Math.pow(2, numBits);
                numBits++;
            }

            if (dictionary[c]) {
                entry = dictionary[c];
            } else {
                if (c === dictSize) {
                    entry = w + w.charAt(0);
                } else {
                    return null;
                }
            }
            result.push(entry);

            dictionary[dictSize++] = w + entry.charAt(0);
            enlargeIn--;

            w = entry;

            if (enlargeIn == 0) {
                enlargeIn = Math.pow(2, numBits);
                numBits++;
            }
        }
    }

    return {
        decompressFromEncodedURIComponent: decompressFromEncodedURIComponent
    };
})();

// ---------- Base64 decoder (pure JS) ----------
function base64ToUint8Array(base64) {
    var clean = base64.replace(/[^A-Za-z0-9+/]/g, '');
    var padding = 0;
    if (clean.endsWith('==')) {
        padding = 2;
    } else if (clean.endsWith('=')) {
        padding = 1;
    }
    clean = clean.replace(/=+$/, '');
    var output = [];
    var buffer = 0, bitsCollected = 0;
    for (var i = 0; i < clean.length; i++) {
        var c = clean.charCodeAt(i);
        var value;
        if (c >= 65 && c <= 90) value = c - 65;
        else if (c >= 97 && c <= 122) value = c - 97 + 26;
        else if (c >= 48 && c <= 57) value = c - 48 + 52;
        else if (c === 43) value = 62;
        else if (c === 47) value = 63;
        else continue;
        buffer = (buffer << 6) | value;
        bitsCollected += 6;
        if (bitsCollected >= 8) {
            bitsCollected -= 8;
            output.push((buffer >> bitsCollected) & 0xFF);
        }
    }
    if (padding > 0) {
        output = output.slice(0, output.length - padding);
    }
    return new Uint8Array(output);
}

// ---------- Helper functions ----------
function dimensionsForPlatform(platform) {
    switch (platform) {
        case 'chalk':
            return { width: 180, height: 180, color: true };
        case 'emery':
            return { width: 200, height: 228, color: true };
        case 'gabbro':
            return { width: 260, height: 260, color: true };
        case 'basalt':
            return { width: 144, height: 168, color: true };
        case 'aplite':
        case 'diorite':
        default:
            return { width: 144, height: 168, color: false };
    }
}

function activeWatchInfo() {
    try {
        return Pebble.getActiveWatchInfo && Pebble.getActiveWatchInfo();
    } catch (e) {
        return null;
    }
}

function activeDimensions() {
    var info = activeWatchInfo();
    var platform = info && info.platform ? info.platform : 'basalt';
    return dimensionsForPlatform(platform);
}

function storageKey(width, height) {
    return CUSTOM_PHOTO_STORAGE_PREFIX + width + 'x' + height;
}

function checksum16(bytes) {
    var sum = 0;
    for (var i = 0; i < bytes.length; i++) {
        sum = (sum + bytes[i]) & 0xFFFF;
    }
    return sum;
}

function saveImage(width, height, pixelData, palette, isColor) {
    var key = storageKey(width, height);
    var record = {
        version: 1,
        width: width,
        height: height,
        length: pixelData.length,
        checksum: checksum16(pixelData),
        pixelData: Array.prototype.slice.call(pixelData),
        palette: palette ? Array.prototype.slice.call(palette) : null,
        color: isColor,
        dirty: true
    };
    volatileImages[key] = record;
    try {
        localStorage.setItem(key, JSON.stringify(record));
    } catch (e) {
        console.log('Could not persist custom photo: ' + e.message);
    }
    return record;
}

function loadImage(width, height) {
    var key = storageKey(width, height);
    if (volatileImages[key]) {
        return volatileImages[key];
    }
    try {
        var stored = localStorage.getItem(key);
        if (stored) {
            var record = JSON.parse(stored);
            volatileImages[key] = record;
            return record;
        }
    } catch (e) {
        console.log('Error reading stored custom photo: ' + e.message);
    }
    return null;
}

function markImageClean(width, height, checksum) {
    var record = loadImage(width, height);
    if (!record || record.checksum !== checksum) return;
    record.dirty = false;
    volatileImages[storageKey(width, height)] = record;
    try {
        localStorage.setItem(storageKey(width, height), JSON.stringify(record));
    } catch (e) {
        console.log('Could not mark custom photo delivered: ' + e.message);
    }
}

function queueMessage(dictionary, onSuccess, onFailure) {
    sendQueue.push({
        dictionary: dictionary,
        attempts: 0,
        onSuccess: onSuccess,
        onFailure: onFailure
    });
    pumpQueue();
}

function pumpQueue() {
    if (sending || sendQueue.length === 0) return;
    sending = true;
    var job = sendQueue[0];

    function attempt() {
        job.attempts++;
        Pebble.sendAppMessage(job.dictionary, function() {
            sendQueue.shift();
            sending = false;
            if (job.onSuccess) job.onSuccess();
            pumpQueue();
        }, function(error) {
            if (job.attempts < MAX_SEND_ATTEMPTS) {
                setTimeout(attempt, 250 * job.attempts);
                return;
            }
            sendQueue.shift();
            sending = false;
            if (job.onFailure) job.onFailure(error);
            pumpQueue();
        });
    }

    attempt();
}

function failTransfer(error) {
    if (transfer && !transfer.cancelled) {
        console.log('Photo transfer failed: ' + JSON.stringify(error || {}));
    }
    transfer = null;
    startPendingTransfer();
}

function cancelPhotoTransfers() {
    photoTransferGeneration++;
    pendingTransfer = null;
    if (transfer) transfer.cancelled = true;
}

function startPendingTransfer() {
    if (!pendingTransfer || transfer) return;
    var pending = pendingTransfer;
    pendingTransfer = null;
    startTransfer(pending.width, pending.height, pending.palette, pending.color);
}

function finishTransfer() {
    if (!transfer) return;
    if (transfer.cancelled) {
        transfer = null;
        startPendingTransfer();
        return;
    }
    var dict = {};
    dict[KEY_IMAGE_END] = 1;
    dict[KEY_IMAGE_LENGTH] = transfer.record.length;
    dict[KEY_IMAGE_CHECKSUM] = transfer.record.checksum;
    queueMessage(dict, function() {
        if (!transfer) return;
        if (transfer.cancelled) {
            transfer = null;
            startPendingTransfer();
            return;
        }
        var completed = transfer;
        console.log('Custom photo transfer complete');
        markImageClean(completed.width, completed.height, completed.record.checksum);
        transfer = null;
        startPendingTransfer();
    }, failTransfer);
}

function sendNextChunk() {
    if (!transfer) return;
    if (transfer.cancelled) {
        transfer = null;
        startPendingTransfer();
        return;
    }
    if (transfer.offset >= transfer.record.pixelData.length) {
        finishTransfer();
        return;
    }

    var end = Math.min(transfer.offset + CUSTOM_CHUNK_SIZE, transfer.record.pixelData.length);
    var chunk = transfer.record.pixelData.slice(transfer.offset, end);

    var dict = {};
    dict[KEY_IMAGE_OFFSET] = transfer.offset;
    dict[KEY_IMAGE_CHUNK] = chunk;
    queueMessage(dict, function() {
        if (!transfer) return;
        if (transfer.cancelled) {
            transfer = null;
            startPendingTransfer();
            return;
        }
        transfer.offset = end;
        sendNextChunk();
    }, failTransfer);
}

function startTransfer(width, height, palette, color, queueIfBusy) {
    if (transfer) {
        if (queueIfBusy) {
            pendingTransfer = { width: width, height: height, palette: palette, color: color };
            console.log('Queued newer custom photo behind active transfer');
        }
        return;
    }
    var record = loadImage(width, height);
    if (!record) {
        console.log('No custom photo stored for ' + width + 'x' + height);
        return;
    }

    transfer = {
        width: width,
        height: height,
        record: record,
        offset: 0,
        cancelled: false
    };

    var dict = {};
    dict[KEY_IMAGE_BEGIN] = 1;
    dict[KEY_IMAGE_WIDTH] = width;
    dict[KEY_IMAGE_HEIGHT] = height;
    dict[KEY_IMAGE_LENGTH] = record.length;
    dict[KEY_IMAGE_CHECKSUM] = record.checksum;
    dict[KEY_IMAGE_PALETTE] = record.palette.slice();
    queueMessage(dict, sendNextChunk, failTransfer);
}

function sendConfiguration(wallpaper, photoRecord, dimensions, transactionAttempt) {
    if (wallpaper !== WALLPAPER_CUSTOM) {
        cancelPhotoTransfers();
    }
    var transferGeneration = photoTransferGeneration;

    var dict = {};
    dict[KEY_WALLPAPER] = wallpaper;
    if (wallpaper === WALLPAPER_CUSTOM && photoRecord) {
        dict[KEY_IMAGE_DESIRED_CHECKSUM] = photoRecord.checksum;
    }

    queueMessage(dict, function() {
        console.log('Configuration sent to watch');
        var selected = parseInt(storedClaySetting('WALLPAPER', '1'), 10);
        if (wallpaper === WALLPAPER_CUSTOM && photoRecord && selected === WALLPAPER_CUSTOM &&
            transferGeneration === photoTransferGeneration) {
            startTransfer(dimensions.width, dimensions.height,
                          photoRecord.palette, photoRecord.color, true);
        }
    }, function(error) {
        console.log('Configuration send failed: ' + JSON.stringify(error || {}));
        if (!photoRecord || transactionAttempt + 1 >= MAX_CONFIG_TRANSACTION_ATTEMPTS) return;
        setTimeout(function() {
            var latest = loadImage(dimensions.width, dimensions.height);
            var selected = parseInt(storedClaySetting('WALLPAPER', '1'), 10);
            if (latest && latest.dirty && selected === WALLPAPER_CUSTOM) {
                sendConfiguration(WALLPAPER_CUSTOM, latest, dimensions, transactionAttempt + 1);
            }
        }, 750 * (transactionAttempt + 1));
    });
}

function syncDirtyPhoto() {
    var dimensions = activeDimensions();
    var record = loadImage(dimensions.width, dimensions.height);
    var wallpaper = parseInt(storedClaySetting('WALLPAPER', '1'), 10);
    if (!record || !record.dirty || wallpaper !== WALLPAPER_CUSTOM) return;
    sendConfiguration(WALLPAPER_CUSTOM, record, dimensions, 0);
}

function storedClaySetting(key, fallback) {
    try {
        var settings = JSON.parse(localStorage.getItem('clay-settings') || '{}');
        var value = settings[key];
        if (value && typeof value === 'object' && value.hasOwnProperty('value')) {
            value = value.value;
        }
        return typeof value === 'undefined' ? fallback : value;
    } catch (e) {
        return fallback;
    }
}

function parseClayResponse(response) {
    var text = response;
    if (text.charAt(0) !== '{') text = decodeURIComponent(text);
    return JSON.parse(text);
}

function clayValue(settings, key) {
    var entry = settings[key];
    if (entry && typeof entry === 'object' && entry.hasOwnProperty('value')) {
        return entry.value;
    }
    return entry;
}

function payloadValue(payload, key) {
    if (payload && typeof payload[key] !== 'undefined') return payload[key];
    return undefined;
}

// ---------- Pebble event handlers ----------
Pebble.addEventListener('ready', function() {
    console.log('PebbleKit JS ready');
    setTimeout(syncDirtyPhoto, 500);
});

Pebble.addEventListener('showConfiguration', function() {
    console.log('showConfiguration triggered');

    var info = activeWatchInfo();
    var platform = info && info.platform ? info.platform : 'basalt';
    var dims = dimensionsForPlatform(platform);
    var configType = dims.color ? 'color' : 'bw';

    configPageUrl = BASE_CONFIG_URL + 'config_' + configType + '.html' +
                    '?platform=' + platform +
                    '&width=' + dims.width +
                    '&height=' + dims.height;
  
    // Try to append version from stored config, if available
    try {
        var storedConfig = JSON.parse(localStorage.getItem('twm_config') || '{}');
        if (storedConfig.Version) {
            configPageUrl += '&version=' + encodeURIComponent(storedConfig.Version);
        }
    } catch (e) {
        // ignore
    }

    console.log('Config URL: ' + configPageUrl);

    waitingForConfig = true;
    console.log('Sending RequestConfig (key 0)');
    Pebble.sendAppMessage({ 0: 1 },
        function() {
            console.log('RequestConfig sent');
        },
        function(e) {
            console.log('RequestConfig failed: ' + JSON.stringify(e));
            waitingForConfig = false;
            openConfigPage();
        }
    );

    if (configTimeout) clearTimeout(configTimeout);
    configTimeout = setTimeout(function() {
        console.log('Config request timed out – opening page with localStorage (if any)');
        waitingForConfig = false;
        openConfigPage();
        configTimeout = null;
    }, 3500);
});

function openConfigPage() {
    if (configPageUrl) {
        console.log('Opening config page: ' + configPageUrl);
        Pebble.openURL(configPageUrl);
        configPageUrl = null;
    }
}

Pebble.addEventListener('appmessage', function(e) {
    console.log('appmessage received: ' + JSON.stringify(e.payload));

    if (e.payload[KEY_CONFIG_DATA]) {
        var json = e.payload[KEY_CONFIG_DATA];
        console.log('ConfigData: ' + json);
        try {
            var config = JSON.parse(json);
            localStorage.setItem('twm_config', JSON.stringify(config));
            console.log('Saved config to localStorage');
    
            // Update config page URL with version info
            watchVersionFromConfig = config.Version || '1.0.0';
            if (configPageUrl && configPageUrl.indexOf('version=') === -1) {
                configPageUrl += '&version=' + encodeURIComponent(watchVersionFromConfig);
            }
        } catch (err) {
            console.log('Error parsing ConfigData: ' + err);
        }
    
        if (waitingForConfig) {
            waitingForConfig = false;
            if (configTimeout) {
                clearTimeout(configTimeout);
                configTimeout = null;
            }
            openConfigPage();
        }
    }

    var request = payloadValue(e.payload, KEY_IMAGE_REQUEST);
    if (request) {
        var wallpaper = parseInt(storedClaySetting('WALLPAPER', '1'), 10);
        if (wallpaper === WALLPAPER_CUSTOM) {
            var dims = activeDimensions();
            var record = loadImage(dims.width, dims.height);
            if (record) {
                startTransfer(dims.width, dims.height, record.palette, record.color, true);
            }
        }
    }
});

Pebble.addEventListener('webviewclosed', function(e) {
    console.log('webviewclosed');
    console.log('Response: ' + e.response);

    var config = null;

    if (e.response) {
        try {
            var decoded = decodeURIComponent(e.response);
            config = JSON.parse(decoded);
            console.log('Parsed config from response: ' + JSON.stringify(config));
        } catch (err) {
            console.log('Could not parse response: ' + err);
        }
    }

    if (!config) {
        var stored = localStorage.getItem('twm_config');
        if (stored) {
            try {
                config = JSON.parse(stored);
                console.log('Using config from localStorage: ' + JSON.stringify(config));
            } catch (e) {
                console.log('Error parsing localStorage: ' + e);
            }
        }
    }

    if (!config) {
        console.log('No config to send');
        return;
    }

    var wallpaper = parseInt(config.Wallpaper !== undefined ? config.Wallpaper : 1, 10);
    if (isNaN(wallpaper)) wallpaper = 1;
    wallpaper = Math.max(0, Math.min(WALLPAPER_CUSTOM, wallpaper));

    var dict = {};
    dict[KEY_WALLPAPER] = wallpaper;
    if (config.ClockMode !== undefined) dict[KEY_CLOCK_MODE] = parseInt(config.ClockMode, 10) || 0;

    // Leading Zeros mode (0/1/2)
    if (config.LeadingZeros !== undefined) {
        var lz = parseInt(config.LeadingZeros, 10);
        if (!isNaN(lz) && lz >= 0 && lz <= 2) dict[KEY_LEADING_ZEROS] = lz;
        else dict[KEY_LEADING_ZEROS] = 2;
    } else {
        dict[KEY_LEADING_ZEROS] = 2;
    }

    // Show AM/PM mode (0/1/2)
    if (config.ShowAMPM !== undefined) {
        var ap = parseInt(config.ShowAMPM, 10);
        if (!isNaN(ap) && ap >= 0 && ap <= 2) dict[KEY_SHOW_AMPM] = ap;
        else dict[KEY_SHOW_AMPM] = 2;
    } else {
        dict[KEY_SHOW_AMPM] = 2;
    }

    // Flick Window (0=None, 1=OneShot, 2=Timer)
    if (config.FlickWindow !== undefined) {
        var fw = parseInt(config.FlickWindow, 10);
        if (!isNaN(fw) && fw >= 0 && fw <= 2) dict[KEY_FLICK_WINDOW] = fw;
        else dict[KEY_FLICK_WINDOW] = 1;
    } else {
        dict[KEY_FLICK_WINDOW] = 1;
    }
    
    if (config.HourlyVibration !== undefined) {
        var hv = parseInt(config.HourlyVibration, 10);
        if (!isNaN(hv) && hv >= 0 && hv <= 3) dict[KEY_HOURLY_VIBRATION] = hv;
        else dict[KEY_HOURLY_VIBRATION] = 0;
    } else {
        dict[KEY_HOURLY_VIBRATION] = 0;
    }
    
    if (config.BTDisconnectVibration !== undefined) {
        var btv = parseInt(config.BTDisconnectVibration, 10);
        if (!isNaN(btv) && btv >= 0 && btv <= 3) dict[KEY_BT_DISCONNECT_VIBRATION] = btv;
        else dict[KEY_BT_DISCONNECT_VIBRATION] = 0;
    } else {
        dict[KEY_BT_DISCONNECT_VIBRATION] = 0;
    }
    
    if (config.HourlyChime !== undefined) {
        dict[KEY_HOURLY_CHIME] = (config.HourlyChime === true || config.HourlyChime === 1) ? 1 : 0;
    } else {
        dict[KEY_HOURLY_CHIME] = 0;
    }
    
    if (config.Inverted !== undefined) dict[KEY_INVERTED] = (config.Inverted === true || config.Inverted === 'true') ? 1 : 0;
    if (config.UI_Color) dict[KEY_UI_COLOR] = config.UI_Color;

    // Save ordinary settings to localStorage
    var ordinarySettings = {
        WALLPAPER: String(wallpaper),
        ClockMode: String(config.ClockMode !== undefined ? config.ClockMode : 0),
        LeadingZeros: String(dict[KEY_LEADING_ZEROS]),
        ShowAMPM: String(dict[KEY_SHOW_AMPM]),
        FlickWindow: String(dict[KEY_FLICK_WINDOW]),
        HourlyVibration: String(dict[KEY_HOURLY_VIBRATION]),
        BTDisconnectVibration: String(dict[KEY_BT_DISCONNECT_VIBRATION]),
        HourlyChime: String(dict[KEY_HOURLY_CHIME]),
        Inverted: String(config.Inverted !== undefined ? config.Inverted : false),
        UI_Color: config.UI_Color || 'aa55ff'
    };
    localStorage.setItem('clay-settings', JSON.stringify(ordinarySettings));

    var dims = activeDimensions();
    var photoRecord = null;
    if (wallpaper === WALLPAPER_CUSTOM) {
        if (config.CustomPhoto && config.CustomPhoto.compressed) {
            try {
                var base64Pixel = LZString.decompressFromEncodedURIComponent(config.CustomPhoto.compressed);
                if (base64Pixel) {
                    var pixelData = base64ToUint8Array(base64Pixel);
                    photoRecord = saveImage(dims.width, dims.height,
                                            pixelData,
                                            config.CustomPhoto.palette,
                                            config.CustomPhoto.color);
                }
            } catch (e) {
                console.log('Failed to decode custom photo: ' + e.message);
            }
        }
        if (!photoRecord) {
            photoRecord = loadImage(dims.width, dims.height);
            if (!photoRecord) {
                console.log('No custom photo available');
            }
        }
    }

    // Send settings first
    queueMessage(dict, function() {
        console.log('Settings sent to watch');
        // After settings are sent, start custom photo transfer if needed
        if (wallpaper === WALLPAPER_CUSTOM && photoRecord) {
            startTransfer(dims.width, dims.height,
                          photoRecord.palette, photoRecord.color, true);
        }
    }, function(error) {
        console.log('Failed to send settings: ' + JSON.stringify(error));
    });
});