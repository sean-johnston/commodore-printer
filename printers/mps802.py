from printers.printer_constants import *
from printers.printer_profile import print_profile
from printers.fonts import graphic_font_8x8
from printers.fonts import business_font_8x8
from printers.fonts import control_character

class mps802(print_profile):
    def __init__(self):
        super().__init__()

        self.LPI8 = 18
        self.LPI6 = 24
        self.LPI4 = 36
        self.LPI1 = 72
        self.y_top         = 0
        self.font_graphic = graphic_font_8x8
        self.font_business = business_font_8x8
        self.font_width    = 8
        self.font_height   = 8
        self.page_width    = 640 # Width of page
        self.page_height   = 720 # 693  # Length of page
        self.line_space_val = self.LPI6 # LPI6
        self.inches_per_page = self.page_height / 11; # 11 Inches
        self.line_space    = self.inches_per_page / ((72 / self.line_space_val ) * 2)
        self.space_between_lines = False
        self.graph_mode    = False
        self.def_char      = False

        self.font_set = self.font_graphic

        self.set_format = False
        self.format     = ""
        self.set_formatting = False
        self.formatting = ""

        self.custom_character = [0,0,0,0,0,0,0,0]

    def set_line_spacing(self, value):
        self.line_space_val = value
        self.line_space    = self.inches_per_page / ((72 / self.line_space_val ) * 2)

    def process_formatting(self):
        keep_chars = ' A9z$S.-'+chr(0xa0)
        # Split string into fields with char 29
        format_string = ''
        for f in self.format:
            if f in keep_chars:
                format_string += f

        # Split format string into fields
        formatting_list = self.formatting.strip().split(chr(29))

        print('format_string  :', format_string)
        print('formatting_list:', formatting_list)

        pass

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
        # If we are in escape mode and the character is one of 
        # the secondary addresses, set the font set

        if ch == ESC:         # When followed by the POS code, start position of dot address
            self.esc = True
            return True

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

            if ch == 0x3f: # End of secondary address
                if self.space_between_lines == True:
                    self.space_between_lines = False

                if self.set_format == True:
                    print(self.format)
                    self.set_format = False

                if self.set_formatting == True:
                    print(self.formatting)
                    self.set_formatting = False
                    self.process_formatting()

                if self.graph_mode == True:
                    self.graph_mode = False

                if self.def_char == True:
                    self.def_char = False
                    while len(self.custom_character) < 8: self.custom_character.append(0)
                return True

            if ch == 1: # Invoke formatting feature
                self.set_formatting = True
                self.formatting = ""
                return True

            if ch == 2: # Store format data
                self.set_format = True
                self.format = ""
                return True

            if ch == 3: # Number of lines per page
                return True

            if ch == 4: # Enable diagnostic messages
                return True

            if ch == 5: # Define programmable character
                self.def_char = True
                self.custom_character = []
                return True

            if ch == 6: # Set spacing between lines
                self.space_between_lines = True
                return True

            if ch == 8: # Graphic mode toggle
                self.graph_mode = True
                return True

            if ch == 9: # Suppress diagnostic messages
                return True

            if ch == 10: # Reset printer
                return True

        if self.esc_esc:
            ch = ESC
            self.esc_esc = False

        if self.set_format == True and ch != ESC:
            self.format += chr(ch)
            return True

        if self.set_formatting == True and ch != ESC:
            self.formatting +=chr(ch)
            return True

        if self.graph_mode and ch != ESC:
            self.parent.output_byte(ch)
            self.parent.x += SIZE * self.char_width
            return True

        if self.def_char:
            if len(self.custom_character) < 8:
                self.custom_character.append(ch)
            return True
        
        if self.space_between_lines == True:
            self.set_line_spacing(ch)
            return True

        if ch == 254:
            for i in self.custom_character:
                self.parent.output_byte(i)
                self.parent.x += SIZE * self.char_width
            return True

        return False

    def post_chout(self, ch, add_data):
        if ch == CURSOR_UP and not self.quote:   # Cursor up mode (Graphic)
            self.font_set = self.font_graphic

        if ch == CURSOR_DOWN and not self.quote: # Cursor down mode (Business)
            self.font_set = self.font_business

        if ch == RVS_ON:      # Reverse on
            self.reverse = True

        if ch == RVS_OFF:     # Revers off
            self.reverse = False

        if ch == FF:          # Form feed
            self.parent.new_page()

        if ch == SO:          # Double width character mode
            self.char_width = DOUBLE_WIDTH
            self.graph_mode = False
            self.set_line_spacing(self.LPI6)

        if ch == SI:          # Standard character mode
            self.char_width = NORMAL_WIDTH
            self.graph_mode = False
            self.set_line_spacing(self.LPI6)
            # TODO: Re-align the output to be an a character boundary

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

