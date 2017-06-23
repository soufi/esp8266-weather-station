/**The MIT License (MIT)

Copyright (c) 2015 by Daniel Eichhorn

Permission is hereby granted, free of charge, to any person obtaining a copy
of this software and associated documentation files (the "Software"), to deal
in the Software without restriction, including without limitation the rights
to use, copy, modify, merge, publish, distribute, sublicense, and/or sell
copies of the Software, and to permit persons to whom the Software is
furnished to do so, subject to the following conditions:

The above copyright notice and this permission notice shall be included in all
copies or substantial portions of the Software.

THE SOFTWARE IS PROVIDED "AS IS", WITHOUT WARRANTY OF ANY KIND, EXPRESS OR
IMPLIED, INCLUDING BUT NOT LIMITED TO THE WARRANTIES OF MERCHANTABILITY,
FITNESS FOR A PARTICULAR PURPOSE AND NONINFRINGEMENT. IN NO EVENT SHALL THE
AUTHORS OR COPYRIGHT HOLDERS BE LIABLE FOR ANY CLAIM, DAMAGES OR OTHER
LIABILITY, WHETHER IN AN ACTION OF CONTRACT, TORT OR OTHERWISE, ARISING FROM,
OUT OF OR IN CONNECTION WITH THE SOFTWARE OR THE USE OR OTHER DEALINGS IN THE
SOFTWARE.

See more at http://blog.squix.ch
*/

#include <ESP8266WiFi.h>
#include <WiFiClient.h>
#include "WundergroundForecast.h"

WundergroundForecast::WundergroundForecast(boolean _isMetric) {
  isMetric = _isMetric;
}

void WundergroundForecast::updateForecast(String apiKey, String language, String country, String city) {
  doUpdate("/api/" + apiKey + "/forecast10day/lang:" + language + "/q/" + country + "/" + city + ".json");
}


void WundergroundForecast::updateForecastPWS(String apiKey, String language, String pws) {
  doUpdate("/api/" + apiKey + "/forecast10day/lang:" + language + "/q/pws:" + pws + ".json");
}

void WundergroundForecast::updateForecastZMW(String apiKey, String language, String zmwCode) {
  doUpdate("/api/" + apiKey + "/forecast10day/lang:" + language + "/q/zmw:" + zmwCode + ".json");
}

void WundergroundForecast::doUpdate(String url) {
  JsonStreamingParser parser;
  parser.setListener(this);
  WiFiClient client;
  const int httpPort = 80;
  if (!client.connect("api.wunderground.com", httpPort)) {
    Serial.println("connection failed");
    return;
  }

  Serial.print("Requesting URL: ");
  Serial.println(url);

  // This will send the request to the server
  client.print(String("GET ") + url + " HTTP/1.1\r\n" +
               "Host: api.wunderground.com\r\n" +
               "Connection: close\r\n\r\n");
  int retryCounter = 0;
  while(!client.available()) {
    delay(1000);
    retryCounter++;
    if (retryCounter > 10) {
      return;
    }
  }

  int pos = 0;
  boolean isBody = false;
  char c;

  int size = 0;
  client.setNoDelay(false);
  while(client.connected()) {
    while((size = client.available()) > 0) {
      c = client.read();
      if (c == '{' || c == '[') {
        isBody = true;
      }
      if (isBody) {
        parser.parse(c);
      }
    }
  }
}

void WundergroundForecast::whitespace(char c) {
  Serial.println("whitespace");
}

void WundergroundForecast::startDocument() {
  Serial.println("start document");
}

void WundergroundForecast::key(String key) {
  currentKey = String(key);
  if (currentKey == "simpleforecast") {
    isSimpleForecast = true;
  }

}

void WundergroundForecast::value(String value) {
  if (currentKey == "local_epoch") {
    localEpoc = value.toInt();
    localMillisAtUpdate = millis();
  }

  if (currentKey == "period") {
    currentForecastPeriod = value.toInt();
  }
// Modified below line to add check to ensure we are processing the 10-day forecast
// before setting the forecastTitle (day of week of the current forecast day).
// (The keyword title is used in both the current observation and the 10-day forecast.)
//		Modified by fowlerk
  // if (currentKey == "title" && currentForecastPeriod < MAX_FORECAST_PERIODS) {				// Removed, fowlerk
  if (currentKey == "title" && currentForecastPeriod < MAX_FORECAST_PERIODS) {
      Serial.println(String(currentForecastPeriod) + ": " + value);
      forecastTitle[currentForecastPeriod] = value;
  }

  // Added forecastText key following...fowlerk, 12/3/16
  if (currentKey == "fcttext" && !isMetric && currentForecastPeriod < MAX_FORECAST_PERIODS) {
      forecastText[currentForecastPeriod] = value;
  }
  // Added option for metric forecast following...fowlerk, 12/22/16
  if (currentKey == "fcttext_metric" && isMetric && currentForecastPeriod < MAX_FORECAST_PERIODS) {
      forecastText[currentForecastPeriod] = value;
  }

  // Added PoP (probability of precipitation) key following...fowlerk, 12/22/16
  if (currentKey == "pop" && currentForecastPeriod < MAX_FORECAST_PERIODS) {
      PoP[currentForecastPeriod] = value;
  }


  // The detailed forecast period has only one forecast per day with low/high for both
  // night and day, starting at index 1.
  int dailyForecastPeriod = (currentForecastPeriod - 1) * 2;

  if (currentKey == "fahrenheit" && !isMetric && dailyForecastPeriod < MAX_FORECAST_PERIODS) {

      if (currentParent == "high") {
        forecastHighTemp[dailyForecastPeriod] = value;
      }
      if (currentParent == "low") {
        forecastLowTemp[dailyForecastPeriod] = value;
      }
  }
  if (currentKey == "celsius" && dailyForecastPeriod < MAX_FORECAST_PERIODS) {

      if (currentParent == "high") {
        Serial.println(String(currentForecastPeriod)+ ": " + value);
        forecastHighTemp[dailyForecastPeriod] = value;
      }
      if (currentParent == "low") {
        forecastLowTemp[dailyForecastPeriod] = value;
      }
  }

  if (currentKey == "month" && isSimpleForecast && currentForecastPeriod < MAX_FORECAST_PERIODS)  {
  	//	Handle transition from txtforecast to simpleforecast, as
  	//	the key "period" doesn't appear until after some of the key values needed and is
  	//	used as an array index.
  	if (isSimpleForecast && currentForecastPeriod == 19) {
  		currentForecastPeriod = 0;
  	}
  	forecastMonth[currentForecastPeriod] = value;
  }

  if (currentKey == "day" && isSimpleForecast && currentForecastPeriod < MAX_FORECAST_PERIODS)  {
  	//	Handle transition from txtforecast to simpleforecast, as
  	//	the key "period" doesn't appear until after some of the key values needed and is
  	//	used as an array index.
  	if (isSimpleForecast && currentForecastPeriod == 19) {
  		currentForecastPeriod = 0;
  	}
  	forecastDay[currentForecastPeriod] = value;
  }
  // end fowlerk add

}

void WundergroundForecast::endArray() {

}


void WundergroundForecast::startObject() {
  currentParent = currentKey;
}

void WundergroundForecast::endObject() {
  currentParent = "";
}

void WundergroundForecast::endDocument() {

}

void WundergroundForecast::startArray() {

}

String WundergroundForecast::getForecastIcon(int period) {
  return getMeteoconIcon(forecastIcon[period]);
}

String WundergroundForecast::getForecastTitle(int period) {
  return forecastTitle[period];
}

String WundergroundForecast::getForecastLowTemp(int period) {
  return forecastLowTemp[period];
}

String WundergroundForecast::getForecastHighTemp(int period) {
  return forecastHighTemp[period];
}

String WundergroundForecast::getForecastDay(int period) {
  return forecastDay[period];
}

String WundergroundForecast::getForecastMonth(int period) {
  return forecastMonth[period];
}

String WundergroundForecast::getForecastText(int period) {
  return forecastText[period];
}
String WundergroundForecast::getPoP(int period) {
  return PoP[period];
}


String WundergroundForecast::getMeteoconIcon(String iconText) {
  if (iconText == "chanceflurries") return "F";
  if (iconText == "chancerain") return "Q";
  if (iconText == "chancesleet") return "W";
  if (iconText == "chancesnow") return "V";
  if (iconText == "chancetstorms") return "S";
  if (iconText == "clear") return "B";
  if (iconText == "cloudy") return "Y";
  if (iconText == "flurries") return "F";
  if (iconText == "fog") return "M";
  if (iconText == "hazy") return "E";
  if (iconText == "mostlycloudy") return "Y";
  if (iconText == "mostlysunny") return "H";
  if (iconText == "partlycloudy") return "H";
  if (iconText == "partlysunny") return "J";
  if (iconText == "sleet") return "W";
  if (iconText == "rain") return "R";
  if (iconText == "snow") return "W";
  if (iconText == "sunny") return "B";
  if (iconText == "tstorms") return "0";

  if (iconText == "nt_chanceflurries") return "F";
  if (iconText == "nt_chancerain") return "7";
  if (iconText == "nt_chancesleet") return "#";
  if (iconText == "nt_chancesnow") return "#";
  if (iconText == "nt_chancetstorms") return "&";
  if (iconText == "nt_clear") return "2";
  if (iconText == "nt_cloudy") return "Y";
  if (iconText == "nt_flurries") return "9";
  if (iconText == "nt_fog") return "M";
  if (iconText == "nt_hazy") return "E";
  if (iconText == "nt_mostlycloudy") return "5";
  if (iconText == "nt_mostlysunny") return "3";
  if (iconText == "nt_partlycloudy") return "4";
  if (iconText == "nt_partlysunny") return "4";
  if (iconText == "nt_sleet") return "9";
  if (iconText == "nt_rain") return "7";
  if (iconText == "nt_snow") return "#";
  if (iconText == "nt_sunny") return "4";
  if (iconText == "nt_tstorms") return "&";

  return ")";
}
