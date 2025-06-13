from printers.printer_constants import *
from printers.printer_profile import print_profile
from printers.fonts import graphic_font_6x7
from printers.fonts import business_font_6x7
from printers.fonts import control_character

class mps801(print_profile):
    def __init__(self):
        super().__init__()
        self.font_graphic = graphic_font_6x7
        self.font_business = business_font_6x7
        self.font_width    = 6
        self.font_height   = 7
        self.page_width    = 480  # Width of page
        self.page_height   = 693  # Length of page
        self.y_top         = 0

        self.pos         = False
        self.pos1        = -1
        self.pos2        = -1

        self.sub         = False

        self.repeat      = -1
        self.quote       = False

        self.line_space  = LPI6
        self.graph_mode  = False

        #self.font_set = self.font_graphic

        self.font_set = self.font_graphic

    def output_character(self, f):
        # Print the character
        for i in range(self.font_width):
            byte = f[i]
            if self.reverse: byte = byte ^ 255
            offset = 0
            self.output_byte(byte)

            # Move the the next character position
            self.x += SIZE * self.char_width

    def output_control(self, ch):
        if self.quote and ch in control_character:
            # Print the character
            for i in range(self.font_width):
                o = control_character[ch]
                f = self.font_set[o]

                byte = f[i]
                byte = byte ^ 255
                offset = 0
                self.output_byte(byte)

                # Move the the next character position
                self.x += SIZE * self.char_width

    def pre_chout(self, ch, add_data):
        if self.sub:
            # If we have not read the repeat yet
            if self.repeat == -1:

                # Set repeat
                self.repeat = ch

                # If repeat is 0, set it to 256
                if self.repeat == 0: self.repeat = 256
            else:
                # if repeat is set, turn off sub
                self.sub = False

                # Count down the repeats
                while self.repeat > 0:
                    # Check if we wrapped
                    if self.x >= self.page_width * SIZE:
                        self.x = 0
                        self.y += (self.line_space * SIZE)

                    # If we are adding to the buffer
                    if add_data:
                        # Check if we are at the end of the page,
                        # and start a new page if we are
                        # TODO: Make sub be able to extend to 
                        # the next page
                        if self.y >= self.page_height * SIZE:
                            self.parent.new_page()

                    # Output the bitmap
                    self.chout(ch, False)

                    # Decrement the repeat
                    self.repeat -= 1

                # Reset the repeat count
                self.repeat = -1
            return True

        # If we are in graphics mode, output the bitmapped byte
        if self.graph_mode and ch & 128 == 128:
            self.parent.output_byte(ch - 128)
            self.x += SIZE * self.char_width 
            return True

        # If we are in positional mode
        if self.pos == True:
            # If the first position is not set, set it
            if self.pos1 == -1:
                self.pos1 = ch
            else:
                # If the second position is not set, set it
                if self.pos2 == -1:
                    self.pos2 = ch

                    #if we are in escape mode, position by dot
                    if self.esc_esc == True:
                        p = self.pos2 + ((self.pos1 & 1) * 512)
                        if p < 479:
                            self.x = SIZE * p
                    # otherwise position by character
                    else:
                        if chr(self.pos1) >= '0' and chr(self.pos1) <= '9' and chr(self.pos2) >= '0' and chr(self.pos2) <= '9':
                            pos_str = chr(self.pos1) + chr(self.pos2)
                            if int(pos_str) < 80:
                                self.x = SIZE * 6 * int(pos_str)
                    # Reset the position value
                    self.pos = False
                    self.pos1 = -1
                    self.pos2 = -1
                    self.esc = False
                    self.esc_esc = False
            return True

        # If we are in escape mode and the character is one of 
        # the secondary addresses, set the font set
        if self.esc == True:
            self.esc = False
            if ch == 0 or ch == 7:
                if self.secondary_address != ch:
                    self.secondary_address = ch
                    if self.secondary_address == self.SEC_ADDR_GRAPHIC:
                        self.font_set = self.font_graphic
                    else:
                        self.font_set = self.font_business
                    return True
        return False

    def post_chout(self, ch, add_data):
        if ch == BS:          # Graphics on
            self.graph_mode = True
            self.line_space  = LPI9 # Change the line spacing to closer together

        if ch == SO:          # Double width character mode
            self.char_width = DOUBLE_WIDTH
            self.graph_mode = False
            self.line_space  = LPI6

        if ch == SI:          # Standard character mode
            self.char_width = NORMAL_WIDTH
            self.graph_mode = False
            self.line_space  = LPI6
            # TODO: Re-align the output to be an a character boundary

        if ch == POS:         # Print start position addressing
            self.pos = True

        if ch == SUB:         # Repeat graphic select command
            self.sub = True

        if ch == RVS_ON:      # Reverse on
            self.reverse = True

        if ch == RVS_OFF:     # Revers off
            self.reverse = False

        if ch == FF:          # Form feed
            self.parent.new_page()

        if ch == CURSOR_UP and not self.quote:   # Cursor up mode (Graphic)
            self.font_set = self.font_graphic

        if ch == CURSOR_DOWN and not self.quote: # Cursor down mode (Business)
            self.font_set = self.font_business

        # Process new line and carriage return
        if ch == NL or ch == CR:
            # Move to beginning of next line
            self.x = 0
            self.y += (self.line_space * SIZE)

            # Reset all the flags
            #if self.secondary_address == self.SEC_ADDR_GRAPHIC:
            #    self.font_set = self.font_graphic
            #else:
            #    self.font_set = self.font_business
            self.reverse = False
            # self.graph_mode = False # I don't think graphic mode gets turned of
            self.pos = False
            self.esc = False
            self.quote = False

    def clear_output(self):
        super().clear_output()
        self.pos         = False
        self.pos1        = -1
        self.pos2        = -1
        self.sub         = False
        self.repeat      = -1

        self.line_space  = LPI6
        self.char_width  = NORMAL_WIDTH
        self.reverse     = False
        self.graph_mode  = False

        self.font_set = self.font_graphic

        self.secondary_address = self.SEC_ADDR_GRAPHIC


