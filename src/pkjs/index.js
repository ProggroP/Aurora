var Clay = require('@rebble/clay');
var clayConfig = require('./config.json');
var clay = new Clay(clayConfig);

var FALLBACK_LAT = 53.75;   // Elmshorn
var FALLBACK_LON = 9.65;

function getStoredSettings() {
  try {
    return JSON.parse(localStorage.getItem('clay-settings') || '{}');
  } catch (e) {
    return {};
  }
}

// WMO weather code -> Icon-Typ (muss zu main.c WX_* passen)
function codeToIcon(code) {
  if (code === 0) {
    return 0; // CLEAR
  }
  if (code === 1 || code === 2) {
    return 1; // PARTLY
  }
  if (code === 3) {
    return 2; // CLOUD
  }
  if (code === 45 || code === 48) {
    return 6; // FOG
  }
  if (code >= 51 && code <= 67) {
    return 3; // RAIN/DRIZZLE
  }
  if (code >= 71 && code <= 77) {
    return 4; // SNOW
  }
  if (code >= 80 && code <= 82) {
    return 3; // RAIN SHOWERS
  }
  if (code === 85 || code === 86) {
    return 4; // SNOW SHOWERS
  }
  if (code >= 95) {
    return 5; // THUNDER
  }
  return 2;
}

function sendWeather(lat, lon) {
  var settings = getStoredSettings();
  var unit = settings.TEMP_FAHRENHEIT ? 'fahrenheit' : 'celsius';

  var url = 'https://api.open-meteo.com/v1/forecast'
    + '?latitude=' + lat
    + '&longitude=' + lon
    + '&current=weather_code'
    + '&daily=temperature_2m_max,temperature_2m_min'
    + '&temperature_unit=' + unit
    + '&timezone=auto&forecast_days=1';

  var req = new XMLHttpRequest();
  req.open('GET', url, true);
  req.onload = function () {
    try {
      var d = JSON.parse(req.responseText);
      var code = d.current.weather_code;
      var hi = Math.round(d.daily.temperature_2m_max[0]);
      var lo = Math.round(d.daily.temperature_2m_min[0]);
      Pebble.sendAppMessage({
        WEATHER_ICON: codeToIcon(code),
        WEATHER_HIGH: hi,
        WEATHER_LOW: lo
      });
    } catch (e) {
      console.log('Aurora: Wetter-Parse-Fehler: ' + e);
    }
  };
  req.onerror = function () {
    console.log('Aurora: Wetter-Request fehlgeschlagen');
  };
  req.send();
}

function update() {
  var settings = getStoredSettings();

  if (settings.USE_FIXED_LOC) {
    var lat = parseFloat(settings.FIXED_LAT);
    var lon = parseFloat(settings.FIXED_LON);
    if (!isNaN(lat) && !isNaN(lon)) {
      sendWeather(lat, lon);
      return;
    }
  }

  navigator.geolocation.getCurrentPosition(
    function (pos) {
      sendWeather(pos.coords.latitude, pos.coords.longitude);
    },
    function (err) {
      console.log('Aurora: Geolocation-Fehler, nutze Fallback');
      sendWeather(FALLBACK_LAT, FALLBACK_LON);
    },
    { enableHighAccuracy: false, maximumAge: 1800000, timeout: 15000 }
  );
}

Pebble.addEventListener('ready', function () {
  update();
});

// Anfrage von der Uhr (REQUEST_UPDATE)
Pebble.addEventListener('appmessage', function (e) {
  update();
});

// Nach dem Schliessen der Clay-Konfiguration neu rechnen
Pebble.addEventListener('webviewclosed', function (e) {
  if (e && e.response && e.response !== 'CANCELLED') {
    update();
  }
});
