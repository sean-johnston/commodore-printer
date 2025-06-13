from printers.printer_constants import *
from printers.printer_profile import print_profile
from printers.fonts import font_1520_uppercase
from printers.fonts import font_1520_lowercase
from printers.fonts import control_character
import re

"""
Notes:
    * Absolute origin resets when a linefeed occurs
    * Quote mode does not does not reset on line wrap
    * Shift-CR does not reset quote mode
    * Scribe is reset when a draw command is executed
    * When printer is reset or starts, 4 squares are draw, with a CR

"""

class vic1520(print_profile):
    def __init__(self):
        super().__init__()
        # Fonts
        self.font_uppercase = font_1520_uppercase
        self.font_lowercase = font_1520_lowercase

        # List of colors
        self.colors = ['black', 'blue', 'green', 'red']

        # Character size factors
        self.char_size_values = [0.5, 1.0, 2.0, 4.0]

        # Printer values
        self.page_width    = 480      # Width of page
        self.page_height   = 999 * 2  # Length of page
        self.line_space    = 0        # Line spacing
        self.mult          = 4        # Multiplier
        self.char_width    = 12       # Width of character
        self.color         = 0        # First color

        # Mode flags
        self.xyplot = False           # X/Y plot mode flag
        self.select_color = False     # Color mode flag
        self.select_char_size = False # Character size mode flag
        self.char_rotation = False    # Character rotation mode flag
        self.scribe_line_mode = False # Scribe line mode flag
        self.set_case = False         # Set case mode flag

        # Set the default character size
        self.set_char_size(1)

        # Set the font to uppercase
        self.set_char_case(0)

        # Clear the secondary address data
        self.secondary_address_data = ""

        self.abs_origin_x = 0
        self.abs_origin_y = 0
        
        self.rel_origin_x = 0
        self.rel_origin_y = 0


    def reset_abs_origin(self):
        self.abs_origin_x += self.x
        self.abs_origin_y += self.y

        self.x = 0
        self.y = 0

        self.rel_origin_x = 0
        self.rel_origin_y = 0

    def set_rel_origin(self):
        self.rel_origin_x = self.x
        self.rel_origin_y = self.y

    def reset_scribe_state(self):
        self.scribe_state = 0

    """
    Move the pen all the way back to the left.
    """
    def move_left_side(self):
        self.x = 0

    """
    Move the paper upward by one line.
    This sets the absolute origin to the new position.
    """
    def line_feed(self):
        self.y += (self.line_space * SIZE)
        self.reset_abs_origin()

    """
    Move to the next line without resetting quote mode.
    """
    def carriage_return(self):
        self.move_left_side()
        self.line_feed()

    """
    Move to the next line and reset quote mode.
    """
    def print_char_cr(self):
        self.carriage_return()
        self.quote = False
        self.esc   = False

    """
    On the computer shift-CR would reset quote mode, like CR, but
    the 1520 apparently doesn't do it like that.
    See disassembly 0c7e do_shcr: jsr move_left_side; beq nextchar
    """
    def print_char_shcr(self):
        self.move_left_side()

    """
    Set the case of the character set

    index = Case index

    0 = Uppercase
    1 = Lowercase
    """
    def set_char_case(self, index):
        if index == 0:
            self.font_set = self.font_uppercase
        elif index == 1:
            self.font_set = self.font_lowercase

    """
    Set the size of the character

    size = Size index

    0 = 80 column
    1 = 40 column
    2 = 20 column
    3 = 10 column
    """
    def set_char_size(self, size):
        self.char_size = self.char_size_values[size]
        self.line_space = 16 * self.char_size

    def draw_line(self, x1, y1, x2, y2, color):
        self.parent.canvas.create_line(x1    , y1    , x2    , y2    , fill=color, width=1)
        self.parent.canvas.create_line(x1 + 1, y1    , x2 + 1, y2    , fill=color, width=1)
        self.parent.canvas.create_line(x1    , y1 + 1, x2    , y2 + 1, fill=color, width=1)
        self.parent.canvas.create_line(x1 + 1, y1 + 1, x2 + 1, y2 + 1, fill=color, width=1)
        self.parent.canvas.update()

    """
    Draw a character and the specified location, with a
    certain scale

    f - Character data
    x - X position
    y - Y position
    mult - Scaling factor

    """
    def draw_character(self, f, x, y, mult):
        # Font format:
        #
        # %EXXXYYYP
        # E - last command in list (1)
        # X - x coordinate (0-7)
        # Y - y coordinate (0-7)
        # P - use pen (1), or just move (0)

        # Set x to the height of the character
        if self.y == 0:
            self.y = self.line_space * SIZE
            y = self.y

        # Set start position
        start_x = 0
        start_y = 0

        # Draw the chararacter
        for i in f:

            # Get the end position
            end_x = ((i & 0x70) >> 4) * mult
            end_y = ((i & 0x0e) >> 1) * mult

            # If the low bit is set, we are drawing
            if i & 1:

                # Set the color
                color = self.colors[self.color % 4]

                # Calculate the start and end positions
                x1 = x + start_x
                y1 = y - start_y
                x2 = x + end_x
                y2 = y - end_y

                # Draw the line
                self.draw_line(x1, y1, x2, y2, color)

            # Make the last postions the first
            start_x = end_x
            start_y = end_y

        #self.parent.canvas.update()

    """
    Output a character

    f - Character data
    """
    def output_character(self, f):
        self.draw_character(f, self.x, self.y, self.mult * self.char_size)

        # Move past the character
        self.x += SIZE * self.char_width * self.char_size

    def output_control(self, ch):
        if self.quote and ch in control_character:
            # Print the character
            o = control_character[ch]
            f = self.font_set[o]

            # Draw the character
            self.draw_character(f, self.x, self.y, self.mult * self.char_size)

            # Underline it
            if self.font_set == self.font_uppercase:
                self.draw_character(self.font_set[220], self.x, self.y, self.mult * self.char_size)
            else:
                self.draw_character(self.font_set[92], self.x, self.y, self.mult * self.char_size)

            # Move past the character
            self.x += SIZE * self.char_width * self.char_size

    """
    Get the number data for from the string, and put it in a list

    data - String containing the data
    """
    def get_number_values(self,data):

        # Remove consecutive spaces
        #data = " ".join(re.sub(' +', '', data))
        data = re.sub("\s\s+", " ", data)

        # Remove spaces between negative sign and number
        data = data.replace("- ", "-")

        # Remove Non-digit (Spaces and negative are ok)
        data = ''.join(c for c in data if c.isdigit() or c == ' ' or c == '-')

        # Return a list of integers
        if data == "": return []
        return [int(d) for d in data.strip().split(" ")]

    """
    Called at the beginning of chout

    ch - Character
    add_data = Add data to buffer

    Return True if something was processed, or False of not
    """
    def pre_chout(self, ch, add_data):
        # If we are in escape mode and the character is one of
        # the secondary addresses
        if ch == ESC:
            self.esc = True
            return True

        if self.esc == True:
            self.esc = False

            if ch == 0x3f: # End of secondary address

                # Get values for the command
                data = self.get_number_values(self.secondary_address_data)
                if len(data) == 0: data.append(0)

                # Process X/Y plot
                if self.xyplot == True:
                    cmd = self.secondary_address_data.strip()[0]
                    if len(data) == 1: data.append(0)
                    if cmd == "H": # Move to start point (0,0)
                        self.x = 0
                        self.y = 0

                    elif cmd == "I": # Set relative origin point (X0,Y0) = current (X,Y)
                        self.set_rel_origin()

                    elif cmd == "M": # Move to position (X,Y) relative to the absolute origin (0,0) (pen up)
                        data[0] *= SIZE
                        data[1] *= SIZE
                        self.x = data[0]
                        self.y = data[1]

                    elif cmd == "D": # Draw to position (X,Y) relative to the absolute origin (0,0) (pen down)
                        data[0] *= SIZE
                        data[1] *= SIZE
                        self.reset_scribe_state()
                        self.draw_line(self.x, self.y, data[0], -data[1], self.colors[self.color % 4])
                        self.x = data[0]
                        self.y = -data[1]

                    elif cmd == "R": # Move to position (X,Y) relative to the origin point (X0,Y0) (pen up)
                        self.x = self.rel_origin_x + data[0]
                        self.y = self.rel_origin_y + -data[1]

                    elif cmd == "J": # Draw to position (X,Y) relative to the origin point (X0,Y0) (pen up)
                        data[0] *= SIZE
                        data[1] *= SIZE
                        new_x = self.rel_origin_x + data[0]
                        new_y = self.rel_origin_y + -data[1]
                        self.reset_scribe_state()
                        self.draw_line(self.x, self.y, new_x, new_y, self.colors[self.color % 4])
                        self.x = new_x
                        self.y = new_y

                    self.xyplot = False

                """
                    switch (mps->command) {
                    case 'H':       /* move to absolute origin */
                        mps->cur_x = 0;
                        mps->cur_y = 0;
                        break;
                    case 'I':       /* set relative origin here */
                        set_rel_origin(mps);
                        break;
                    case 'M':       /* move to (x,y) relative to absolute origin */
                        mps->cur_x = mps->command_x;
                        mps->cur_y = mps->command_y;
                        break;
                    case 'R':       /* move to (x,y) relative to relative origin */
                        mps->cur_x = mps->rel_origin_x + mps->command_x;
                        mps->cur_y = mps->rel_origin_y + mps->command_y;
                        break;
                    case 'D':       /* draw to (x,y) relative to absolute origin */
                        new_x = mps->command_x;
                        new_y = mps->command_y;
                        reset_scribe_state(mps);
                        draw(mps, mps->cur_x, mps->cur_y, new_x, new_y);
                        mps->cur_x = new_x;
                        mps->cur_y = new_y;
                        break;
                    case 'J':       /* draw to (x,y) relative to relative origin */
                        new_x = mps->rel_origin_x + mps->command_x;
                        new_y = mps->rel_origin_y + mps->command_y;
                        reset_scribe_state(mps);
                        draw(mps, mps->cur_x, mps->cur_y, new_x, new_y);
                        mps->cur_x = new_x;
                        mps->cur_y = new_y;
                        break;
                """

                # Process color
                if self.select_color == True:
                    self.color = data[0]
                    self.select_color = False

                # Process character size
                if self.select_char_size == True:
                    self.set_char_size(data[0])
                    self.select_char_size = False

                # Process character rotation
                if self.char_rotation == True:
                    self.char_rotation = False

                # Process scribe line mode
                if self.scribe_line_mode == True:
                    self.scribe_line_mode = False

                # Process set case
                if self.set_case == True:
                    self.set_char_case(data[0])
                    self.set_case = False

                return True

            if ch == 1: # X/Y plot
                self.xyplot = True
                self.secondary_address_data = ""
                return True

            if ch == 2: # Select color
                self.select_color = True
                self.secondary_address_data = ""
                return True

            if ch == 3: # Select character size
                self.select_char_size = True
                self.secondary_address_data = ""
                return True

            if ch == 4: # Character rotation
                self.char_rotation = True
                self.secondary_address_data = ""
                return True

            if ch == 5: # Scribe line mode
                self.scribe_line_mode = True
                self.secondary_address_data = ""
                return True

            if ch == 6: # Upper/Lower Case
                self.set_case = True
                self.secondary_address_data = ""
                return True

            if ch == 7: # Reset Printer
                return True

        # Escape an escape
        if self.esc_esc:
            ch = ESC
            self.esc_esc = False
            return False

        # If we are in any of the modes, append the
        # character to the secondary address data
        if (    self.xyplot or
                self.select_color or
                self.select_char_size or
                self.char_rotation or
                self.scribe_line_mode or
                self.set_case
            ):
            self.secondary_address_data += chr(ch)
            return True

        return False

    """
    Called at the end of chout

    ch - Character
    add_data - Add to buffer
    """
    def post_chout(self, ch, add_data):
        if ch == FF:          # Form feed
            self.parent.new_page()

        if ch == CR or ch == NL:
            self.print_char_cr()

        if ch == CR + 128:
            self.print_char_shcr()

    """
    Clear the output
    """
    def clear_output(self):
        super().clear_output()

