
#define COOLING  55

// SPARKING: What chance (out of 255) is there that a new spark will be lit?
// Higher chance = more roaring fire.  Lower chance = more flickery fire.
// Default 120, suggested range 50-200.
#define SPARKING 200

void Fire2012()
{
  int sparking = (int) ((float) SPARKING * (float) brightness / 100.0);
// Array of temperature readings at each simulation cell
  static byte heat[NUM_LEDS];

  // Step 1.  Cool down every cell a little
    for( int i = 0; i < NUM_LEDS; i++) {
      heat[i] = qsub8( heat[i],  random8(0, ((COOLING * 10) / NUM_LEDS) + 2));
    }
  
    // Step 2.  Heat from each cell drifts 'up' and diffuses a little
    for( int k= NUM_LEDS - 1; k >= 2; k--) {
      heat[k] = (heat[k - 1] + heat[k - 2] + heat[k - 2] ) / 3;
    }
    
    // Step 3.  Randomly ignite new 'sparks' of heat near the bottom
    if( random8() < sparking ) {
      int y = random8(7);
      heat[y] = qadd8( heat[y], random8(160,255) );
    }

    // Step 4.  Map from heat cells to LED colors
    for( int j = 0; j < NUM_LEDS; j++) {
      CRGB color = HeatColor( heat[j]);
      int pixelnumber;
      if( gReverseDirection ) {
        pixelnumber = (NUM_LEDS-1) - j;
      } else {
        pixelnumber = j;
      }
      leds[pixelnumber] = color;
    }
}



void rainbow(int range) 
{
  // FastLED's built-in rainbow generator
  //fill_rainbow( leds, NUM_LEDS - range, gHue, 7);
  CHSV hsv;
  hsv.hue = range * 3;
  hsv.val = 255;
  hsv.sat = 240;
  for( int i = 0; i < NUM_LEDS; i++) {
      if(i < range){
        leds[getIndex(i)] = hsv;
        hsv.hue += 1;
      } else {
        leds[getIndex(i)] = CRGB::Black;
      }
  }
}

int getIndex(int i) 
{
  //return NUM_LEDS - i - 1;
  return i;
}

