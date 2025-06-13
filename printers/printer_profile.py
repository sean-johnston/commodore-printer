from printers.printer_constants import *

class print_profile:
    def __init__(self):
        self.x = 0
        self.y = 0

        self.esc         = False
        self.esc_esc     = False
        self.quote       = False
        self.parent      = None
        #self.font_set    = None
        self.reverse     = False
        self.char_width  = NORMAL_WIDTH
        self.line_space  = LPI6

        self.SEC_ADDR_GRAPHIC = 0
        self.SEC_ADDR_BUSNESS = 7

        self.secondary_address = self.SEC_ADDR_GRAPHIC
        #self.font_set = None


    def pre_chout(self, ch, add_data):
        return False

    def post_chout(self, ch, add_data):
        pass

    def set_parent(self, parent):
        self.parent = parent

    def clear_output(self):
        self.esc         = False
        self.quote       = False

    def output_byte(self, byte):
        self.parent.output_byte(byte)

    # Output one charactor to the canvas and image
    def chout(self, ch, add_data = True):
        page_width = self.page_width
        page_height = self.page_height

        # If we want to append to the buffer
        # Set to False if we are redrawing
        if add_data:
            # If the current page does not match the last page, 
            # change to it
            if self.parent.page_current != self.parent.page_last:
                self.page_current = self.parent.page_last
                self.parent.page.current(self.parent.page_last)
                self.parent.clear_canvas()
                self.parent.redraw_page()
                self.parent.page_current = self.parent.page_last
                self.x = self.parent.x_last
                self.y = self.parent.y_last

            # If we reach the end of the page, make a
            # new page
            # Part of the line is at the bottom of the page
            if self.y + 6 >= page_height * SIZE:
                self.parent.new_page()

            # Append the character if it is not a formfeed
            #if ch != FF:
            self.parent.page_data[self.parent.page_current].append(ch)

        if self.esc:
            if ch == ESC:
                self.esc_esc = True
                self.esc = False
                return

        if self.pre_chout(ch, add_data):
            return

        # if the character is in the font
        if ch in self.font_set:

            # Toggle quote
            if ch == QUOTE:
                self.quote = not self.quote

            # Check if we wrapped
            if self.x >= page_width * SIZE:
                self.x = 0
                self.y += (self.line_space * SIZE)

            # If we are adding to the buffer
            if add_data:
                # Check if we are at the end of the page,
                # and start a new page if we are
                if self.y >= page_height * SIZE:
                    self.parent.new_page()

            # Get the dot data of the character
            f = self.font_set[ch]

            # Print the character
            self.output_character(f)

        # If it is not a font character, it must be a control
        # character
        else:
            self.output_control(ch)

            if ch == ESC:         # When followed by the POS code, start position of dot address
                self.esc = True

            self.post_chout(ch, add_data)

        # If we are appending data, set the last x, y, and page
        if add_data:
            self.parent.x_last = self.x
            self.parent.y_last = self.y
            self.parent.page_last = self.parent.page_current
            self.parent.set_scroll()

