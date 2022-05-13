// Include Elegoo touchscreen display libraries
#include <Elegoo_GFX.h>    // Core graphics library
#include <Elegoo_TFTLCD.h> // Hardware-specific library
#include <TouchScreen.h>

#include <Wire.h>
#include <Adafruit_PWMServoDriver.h>

#if defined(__SAM3X8E__)
    #undef __FlashStringHelper::F(string_literal)
    #define F(string_literal) string_literal
#endif

/*
 * ----------------------------------------
 *   ELEGOO TOUCHSCREEN/LCD CONFIGURATION
 * ----------------------------------------
 */

 // Define pin numbers for touchscreen data transfer
#define YP  A3
#define XM  A2
#define YM  9
#define XP  8

// Define minimum/maximum touchscreen ranges
#define TS_MINX  120
#define TS_MAXX  900

#define TS_MINY  70
#define TS_MAXY  920

#define MINPRESSURE  10
#define MAXPRESSURE  1000

// Create the touchscreen instance (with a default resistance of 300 Ohms)
TouchScreen touchscreen(XP, YP, XM, YM, 300);

// Define pin numbers for LCD data transfer 
#define LCD_CS     A3
#define LCD_CD     A2
#define LCD_WR     A1
#define LCD_RD     A0
#define LCD_RESET  A4

// Create the LCD screen instance
Elegoo_TFTLCD screen(LCD_CS, LCD_CD, LCD_WR, LCD_RD, LCD_RESET);

/*
 * -------------------------------------------------
 *   DISPLAY GEOMETRY AND CONTROL CONSTANTS/MACROS
 * -------------------------------------------------
 */

 // Define width/height of display in pixels
#define WIDTH   320
#define HEIGHT  240

// Define custom RGB565 values for colors used in the display program
#define FOREGROUND  0x0000
#define BACKGROUND  0xFFFF
#define GRAY        0xF79E 
#define BLUE        0x869D

// Define information about the geometry of the font for the display
#define LETTER_WIDTH   6
#define LETTER_HEIGHT  9

// General Constants
#define MARGIN              10
#define BUTTON_HEIGHT       36
#define HEADER_TEXT_OFFSET  14
#define HEADER_H            30
#define LARGE_RECT_RADIUS   12
#define SMALL_RECT_RADIUS   6

// State -1 - Help
#define INSTRUCTION_LINE_H 24

// State 0 - Initial Order
#define START_ORDER_X  (MARGIN * 4)
#define START_ORDER_Y  ((HEIGHT / 2) - BUTTON_HEIGHT)
#define START_ORDER_W  (WIDTH - (8 * MARGIN))
#define START_ORDER_H  (2 * BUTTON_HEIGHT)
#define HELP_RADIUS    15

// State 1 - Seasoning Selection
#define SEASONING_X              (MARGIN)
#define SEASONING_W              (WIDTH - (2 * MARGIN))
#define SEASONING_H              (BUTTON_HEIGHT)
#define SEASONING_CIRCLE_RADIUS  6

#define BEEF_Y     (HEADER_H + MARGIN)
#define CHICKEN_Y  (BEEF_Y + BUTTON_HEIGHT + MARGIN)
#define PORK_Y     (CHICKEN_Y + BUTTON_HEIGHT + MARGIN)

#define ACTION_BUTTON_Y       (HEIGHT - MARGIN - BUTTON_HEIGHT)
#define ACTION_BUTTON_W       ((WIDTH / 2) - MARGIN - (MARGIN / 2))
#define ACTION_BUTTON_H       (BUTTON_HEIGHT)
#define CANCEL_X              (MARGIN)
#define SUBMIT_X              ((WIDTH / 2) + (MARGIN / 2))
#define ACTION_BUTTON_TEXT_Y  (HEIGHT - MARGIN - (BUTTON_HEIGHT / 2))
#define CANCEL_TEXT_X         (MARGIN + (ACTION_BUTTON_W / 2))
#define SUBMIT_TEXT_X         (WIDTH - MARGIN - (ACTION_BUTTON_W / 2))

// 0 - "Start Order"
// 1 - Seasonings and "Submit"
// 2 - Pour water
// 3 - Dispense noodles
// 4 - Dispense seasonings
// 5 - Stir/Cook
// 6 - "Done!"
// -1 - Help
int state = 0;

/*
 * ------------------------
 *   SEASONING SERVO SETUP
 * ------------------------
 */

Adafruit_PWMServoDriver pwm = Adafruit_PWMServoDriver();
#define SERVO_FREQ 50

const uint16_t SERVO_MINS[] = {110, 127, 120}; //affects down position
const uint16_t SERVO_MAXS[] = {485, 485, 600}; //affectss up 

/*
 * --------------------
 *   WATER PUMP SETUP
 * --------------------
 */

#define PUMP_PIN 24

/**
 * Perform initial setup on the touchscreen/LCD
 * and initialize the program values.
 */
void setup() {
    // Load information from the screen to identify it
    screen.reset();
    uint16_t identifier = screen.readID();
    if (identifier != 0x9325 &&
        identifier != 0x9328 &&
        identifier != 0x4535 &&
        identifier != 0x7575 &&
        identifier != 0x9341 &&
        identifier != 0x8357) {
        // If we couldn't identify the display, use a default value
        identifier = 0x9341;
    }

    // Initialize the LCD and set it to display horizontally
    screen.begin(identifier);
    screen.setRotation(1);

    // Set up servo motors
    pwm.begin();
    pwm.setOscillatorFrequency(27000000);
    pwm.setPWMFreq(SERVO_FREQ);

    // Set up pump pin
    pinMode(PUMP_PIN, OUTPUT);
    digitalWrite(PUMP_PIN, HIGH);

    // Set the initial values for all program values
    resetMachine();
}

/*
 * ---------------------------------------
 *   PROGRAM VALUES AND ORDER PARAMETERS
 * ---------------------------------------
 */

 // When we poll for a new touched point, this is where it goes
TSPoint selectedPoint;

// When this is true, we redraw the screen on the next loop() pass
bool redraw = true;

// These track which seasonings have been selected
bool beefSelected = false;
bool chickenSelected = false;
bool porkSelected = false;

/**
 * Render the information to the display
 * and handle any touch inputs.
 */
void loop() {
    // Set pin modes for touchscreen pins
    pinMode(XM, OUTPUT);
    pinMode(YP, OUTPUT);
    
    // If we need to redraw, do it
    if (redraw) {
        redraw = false;

        // Start by filling in the screen with white
        screen.fillScreen(BACKGROUND);

        // Depending on the state, draw buttons or text
        switch (state) {
        // Display help
        case -1:
            drawHeader("HELP");

            // Draw instructions
            drawHelpInstructions();

            // Draw back button
            screen.fillRoundRect(SUBMIT_X, ACTION_BUTTON_Y, ACTION_BUTTON_W, ACTION_BUTTON_H, SMALL_RECT_RADIUS, GRAY);
            drawCenteredText("BACK", 2, SUBMIT_TEXT_X, ACTION_BUTTON_TEXT_Y);
            break;
        // Initial state (no order started)
        case 0:
            drawHeader("WELCOME");

            // Draw start order button
            screen.fillRoundRect(START_ORDER_X, START_ORDER_Y, START_ORDER_W, START_ORDER_H, LARGE_RECT_RADIUS, BLUE);
            drawCenteredText("START ORDER", 2, WIDTH / 2, HEIGHT / 2);

            // Draw help button
            screen.drawCircle(WIDTH - MARGIN - HELP_RADIUS, HEIGHT - MARGIN - HELP_RADIUS, HELP_RADIUS, GRAY);
            drawCenteredColoredText("?", 2, GRAY, WIDTH - MARGIN - HELP_RADIUS + 1, HEIGHT - MARGIN - HELP_RADIUS + 1);
            break;
        // Selecting seasonings...
        case 1:
            drawHeader("SELECT SEASONING(S)");

            // Draw seasoning options
            drawSeasoningButton("BEEF", BEEF_Y, beefSelected);
            drawSeasoningButton("CHICKEN", CHICKEN_Y, chickenSelected);
            drawSeasoningButton("PORK", PORK_Y, porkSelected);

            // Draw cancel button
            screen.fillRoundRect(CANCEL_X, ACTION_BUTTON_Y, ACTION_BUTTON_W, ACTION_BUTTON_H, SMALL_RECT_RADIUS, GRAY);
            drawCenteredText("CANCEL", 2, CANCEL_TEXT_X, ACTION_BUTTON_TEXT_Y);

            // Draw place order button (if nothing is selected, gray it out)
            if (beefSelected || chickenSelected || porkSelected) {
                screen.fillRoundRect(SUBMIT_X, ACTION_BUTTON_Y, ACTION_BUTTON_W, ACTION_BUTTON_H, SMALL_RECT_RADIUS, BLUE);
                drawCenteredText("START", 2, SUBMIT_TEXT_X, ACTION_BUTTON_TEXT_Y);
            } else {
                screen.drawRoundRect(SUBMIT_X, ACTION_BUTTON_Y, ACTION_BUTTON_W, ACTION_BUTTON_H, SMALL_RECT_RADIUS, GRAY);
                drawCenteredText("START", 2, SUBMIT_TEXT_X, ACTION_BUTTON_TEXT_Y);
            }
            break;
        // Pouring water...
        case 2:
            drawHeader("PREPARING");
            drawCenteredText("POURING WATER...", 2, WIDTH / 2, HEIGHT / 2);
            break;
        // Adding noodles...
        case 3:
            drawHeader("PREPARING");
            drawCenteredText("ADDING NOODLES...", 2, WIDTH / 2, HEIGHT / 2);
            break;
        // Adding in seasonings...
        case 4:
            drawHeader("PREPARING");
            drawCenteredText("ADDING IN SEASONINGS...", 2, WIDTH / 2, HEIGHT / 2);
            break;
        // Cooking and stirring...
        case 5:
            drawHeader("PREPARING");
            drawCenteredText("COOKING...", 2, WIDTH / 2, HEIGHT / 2);
            drawCenteredText("(PLEASE WAIT)0", 2, WIDTH / 2, HEIGHT / 2 + (LETTER_HEIGHT * 3));
            break;
        // Order done
        case 6:
            drawHeader("ORDER COMPLETE");
            drawCenteredText("ENJOY!", 2, WIDTH / 2, HEIGHT / 2);
            break;
        }
    }

    // Retrieve the next touched point
    selectedPoint = touchscreen.getPoint();

    // If we are in a state that needs user input, compare X/Y touch points
    // If we're past the point of needing input, cycle through remaining states and exit
    if (state < 2) {
        // Scale the touchscreen point to pixels (X/Y have to get swapped because of the orientation of the screen)
        int y = map(selectedPoint.x, TS_MINX, TS_MAXX, HEIGHT, 0);
        int x = map(selectedPoint.y, TS_MINY, TS_MAXY, WIDTH, 0);
    
        // If enough pressure was applied to count as a tap, check for "collision" with any buttons on the current state
        if (selectedPoint.z > MINPRESSURE && selectedPoint.z < MAXPRESSURE) {
            switch (state) {
            // Display help
            case -1:
                if (x > SUBMIT_X && x < (SUBMIT_X + ACTION_BUTTON_W) && y > ACTION_BUTTON_Y && y < (ACTION_BUTTON_Y + ACTION_BUTTON_H)) {
                    state = 0;
                    redraw = true;
                }
                break;
            // Initial state (no order started)
            case 0:
                // If start order is selected, move on to seasoning selection
                if (x > START_ORDER_X && x < (START_ORDER_X + START_ORDER_W) && y > START_ORDER_Y && y < (START_ORDER_Y + START_ORDER_H)) {
                    state = 1;
                    redraw = true;
                } else if (x > (WIDTH - MARGIN - (2 * HELP_RADIUS)) && x < (WIDTH - MARGIN) && y > (HEIGHT - MARGIN - (2 * HELP_RADIUS)) && y < (HEIGHT - MARGIN)) {
                    state = -1;
                    redraw = true;
                }
                break;
            // Selecting seasonings...
            case 1:
                // If cancel is selected, reset
                // If a seasoning is selected, toggle it
                // If submit is selected, move on (as long as at least one seasoning is selected)
                if (x > CANCEL_X && x < (CANCEL_X + ACTION_BUTTON_W) && y > ACTION_BUTTON_Y && y < (ACTION_BUTTON_Y + ACTION_BUTTON_H)) {
                    resetMachine();
                } else if (x > SEASONING_X && x < (SEASONING_X + SEASONING_W) && y > BEEF_Y && y < (BEEF_Y + SEASONING_H)) {
                    beefSelected = !beefSelected;
                    redraw = true;
                } else if (x > SEASONING_X && x < (SEASONING_X + SEASONING_W) && y > CHICKEN_Y && y < (CHICKEN_Y + SEASONING_H)) {
                    chickenSelected = !chickenSelected;
                    redraw = true;
                } else if (x > SEASONING_X && x < (SEASONING_X + SEASONING_W) && y > PORK_Y && y < (PORK_Y + SEASONING_H)) {
                    porkSelected = !porkSelected;
                    redraw = true;
                } else if ((beefSelected || chickenSelected || porkSelected) && (x > SUBMIT_X && x < (SUBMIT_X + ACTION_BUTTON_W) && y > ACTION_BUTTON_Y && y < (ACTION_BUTTON_Y + ACTION_BUTTON_H))) {
                    state = 2;
                    redraw = true;
                }
                break;
            }
        }
    } else if (state == 2) {
        // Pouring water...
        
        delay(1000);

        dispenseWater();

        delay(1000);
        
        // Next state
        state = 4;
        redraw = true;
    } else if (state == 3) {
        // Adding noodles...

        // TODO
        delay(1000);
        state = 4;
        redraw = true;
    } else if (state == 4) {
        // Adding in seasonings...
      
        delay(1000);

        // Turn all selected servos
        if (beefSelected)
          servo_turn_down(0);
        if (chickenSelected)
          servo_turn_down(1);
        if (porkSelected)
          servo_turn_down(2);
        
        delay(750);

        // Turn back all selected servos
        if (beefSelected)
          servo_turn_up(0);
        if (chickenSelected)
          servo_turn_up(1);
        if (porkSelected)
          servo_turn_up(2);
          
        delay(1000);

        // Go to next state
        state = 5;
        redraw = true;
    } else if (state == 5) {
        // Cooking...

        // TODO
        delay(10000);
        state = 6;
        redraw = true;
    } else if (state == 6) {
        // Order done

        // Wait 4 seconds and reset
        delay(4000);
        resetMachine();
    } else {
        // Unknown state, so reset
        resetMachine();
    }
}

/**
 * Resets the machine back to the initial state,
 * and resets all order parameters.
 */
void resetMachine() {
    // Unselect any seasonings
    beefSelected = false;
    chickenSelected = false;
    porkSelected = false;
    // Reset the state and redraw
    state = 0;
    redraw = true;
}

/**
 * Draws the header at the top of the display.
 *
 * @param string the text to display in the header
 */
void drawHeader(String string) {
    // Draw the gray background and center the text on top
    screen.fillRect(0, 0, WIDTH, HEADER_H, GRAY);
    drawCenteredText(string, 2, WIDTH / 2, HEADER_TEXT_OFFSET);
}

/**
 * Draws a seasoning selection button.
 *
 * @param string the name of the seasoning
 * @param y the top-left Y coordinate for the button
 * @param selected whether or not the seasoning is currently enabled
 */
void drawSeasoningButton(String string, int y, bool selected) {
    // If it's selected, draw it blue, otherwise draw it gray
    if (!selected) {
        // Draw gray box and circle
        screen.drawRoundRect(SEASONING_X, y, SEASONING_W, SEASONING_H, SMALL_RECT_RADIUS, GRAY);
        screen.drawCircle(SEASONING_X + MARGIN + SEASONING_CIRCLE_RADIUS, y + (BUTTON_HEIGHT / 2), SEASONING_CIRCLE_RADIUS, GRAY);
    } else {
        // Draw thicker blue line and filled circle
        screen.drawRoundRect(SEASONING_X - 1, y - 1, SEASONING_W + 2, SEASONING_H + 2, SMALL_RECT_RADIUS + 2, BLUE);
        screen.drawRoundRect(SEASONING_X, y, SEASONING_W, SEASONING_H, SMALL_RECT_RADIUS, BLUE);
        screen.drawRoundRect(SEASONING_X + 1, y + 1, SEASONING_W - 2, SEASONING_H - 2, SMALL_RECT_RADIUS - 1, BLUE);
        screen.fillCircle(SEASONING_X + MARGIN + SEASONING_CIRCLE_RADIUS, y + (BUTTON_HEIGHT / 2), SEASONING_CIRCLE_RADIUS, BLUE);
    }

    // Draw the text on top
    drawCenteredText(string, 2, WIDTH / 2, y + (BUTTON_HEIGHT / 2));
}

/**
 * Draws centered text above the specified point.
 *
 * @param string the text to center
 * @param pixelScale the number of desired real pixels in a font "pixel"
 * @param x the X coordinate to center the text over
 * @param y the Y coordinate to center the text over
 */
void drawCenteredText(String string, int pixelScale, int x, int y) {
    // Set the text color and size
    screen.setTextColor(FOREGROUND);
    screen.setTextSize(pixelScale);
    // Draw the text centered directly over the provided point
    screen.setCursor((int) (x - ((string.length() * (LETTER_WIDTH * pixelScale)) / 2.0)), (int) (y - ((LETTER_HEIGHT * pixelScale) / 2.0) + pixelScale));
    screen.println(string);
}

/**
 * Draws centered text above the specified point.
 *
 * @param string the text to center
 * @param pixelScale the number of desired real pixels in a font "pixel"
 * @param color the color to use
 * @param x the X coordinate to center the text over
 * @param y the Y coordinate to center the text over
 */
void drawCenteredColoredText(String string, int pixelScale, int color, int x, int y) {
    // Set the text color and size
    screen.setTextColor(color);
    screen.setTextSize(pixelScale);
    // Draw the text centered directly over the provided point
    screen.setCursor((int) (x - ((string.length() * (LETTER_WIDTH * pixelScale)) / 2.0)), (int) (y - ((LETTER_HEIGHT * pixelScale) / 2.0) + pixelScale));
    screen.println(string);
}

/**
 * Draws left-aligned text next to the specified point.
 *
 * @param string the text to center
 * @param pixelScale the number of desired real pixels in a font "pixel"
 * @param x the X coordinate to align the text to
 * @param y the Y coordinate to align the text to
 */
void drawLeftAlignedText(String string, int pixelScale, int x, int y) {
    // Set the text color and size
    screen.setTextColor(FOREGROUND);
    screen.setTextSize(pixelScale);
    // Draw the text left-aligned to the X coordinate and centered over the Y coordinate
    screen.setCursor(x, (int) (y - ((LETTER_HEIGHT * pixelScale) / 2.0) + pixelScale));
    screen.println(string);
}

/**
 * Draws the help instructions for the program
 * when on the help state.
 */
void drawHelpInstructions() {
    drawLeftAlignedText("1. Select \"Start Order\"", 2, MARGIN, HEADER_H + MARGIN + (INSTRUCTION_LINE_H / 2));
    drawLeftAlignedText("2. Choose seasoning(s)", 2, MARGIN, HEADER_H + MARGIN + INSTRUCTION_LINE_H + (INSTRUCTION_LINE_H / 2));
    drawLeftAlignedText("3. Select \"Start\"", 2, MARGIN, HEADER_H + MARGIN + (2 * INSTRUCTION_LINE_H) + (INSTRUCTION_LINE_H / 2));
    drawLeftAlignedText("4. Wait for your noodles", 2, MARGIN, HEADER_H + MARGIN + (3 * INSTRUCTION_LINE_H) + (INSTRUCTION_LINE_H / 2));
    drawLeftAlignedText("5. Enjoy!", 2, MARGIN, HEADER_H + MARGIN + (4 * INSTRUCTION_LINE_H) + (INSTRUCTION_LINE_H / 2));
}

void servo_turn_up(int servo_num) {
  for (uint16_t pulselen = SERVO_MINS[servo_num]; pulselen < SERVO_MAXS[servo_num]; pulselen++) {
    pwm.setPWM(servo_num, 0, pulselen);
    //delay(1);
  }
}

void servo_turn_down(int servo_num) {
  for (uint16_t pulselen = SERVO_MAXS[servo_num]; pulselen > SERVO_MINS[servo_num]; pulselen--) {
    pwm.setPWM(servo_num, 0, pulselen);
    //delay(1);
  }
}

void dispenseWater() {
  digitalWrite(PUMP_PIN, LOW);
  delay(3000);
  digitalWrite(PUMP_PIN, HIGH);
}
