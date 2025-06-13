#!/usr/bin/env python3
import tkinter as tk
from tkinter import ttk
from tkinter import filedialog
from tkinter.constants import NW
from PIL import Image, ImageDraw
import serial
import serial.tools.list_ports
import sys
import os
import getopt
import platform

from pynput.keyboard import Key, Controller

from printers.mps801 import mps801
from printers.mps802 import mps802
from printers.vic1520 import vic1520

from printers.printer_constants import *

DEFAULT_PRINTER = "VIC 1520"

class Printer:
    def __init__(self):
        self.printer_profile = None

        self.img = None

        self.page_current = 0
        self.page_data = []
        self.page_last = 0

        # Add buffer for the current page
        self.page_data.append(bytearray())
        self.page_data[self.page_current].append(0)

        self.keyboard = Controller()

        self.printer_selected = DEFAULT_PRINTER

        self.create_ui()

    def select_printer(self, printer):
        if printer == 'MPS 801':
            self.printer_profile = mps801()
        if printer == 'MPS 802':
            self.printer_profile = mps802()
        if printer == 'VIC 1520':
            self.printer_profile = vic1520()

        self.printer_profile.set_parent(self)

    def refresh_ui(self):
        x = self.root.winfo_x() - 12
        y = self.root.winfo_y() - 90

        page_width = self.printer_profile.page_width
        page_height = self.printer_profile.page_height
        self.root.title(self.printer_select.get())
        self.root.geometry("{width}x{height}".format(width=(page_width*SIZE)+25,height=MIN_Y, x=x, y=y))
        self.root.resizable(False,True)
        self.root.minsize(500,MIN_Y)
        self.root.maxsize((page_width*SIZE)+25,2000)

        # Create an image to draw the output on
        #white = 
        self.image = None
        self.image = Image.new(
            "RGB",
            (self.printer_profile.page_width*OUTPUT_SIZE,self.printer_profile.page_height*OUTPUT_SIZE),
            color = (255, 255, 255) # type: ignore
        ) # type: ignore

        # Load the bitmap to draw the pixel
        self.draw = ImageDraw.Draw(self.image)

    def child_widgets(self, width, height, size, output_size):
        # Create the frame to contain the combobox
        self.control_frame=tk.Frame()
            #self.root,width=self.printer_profile.font_width*SIZE,height=100,padx=5,pady=5)
        self.control_frame.pack(expand=True, fill=tk.BOTH)

        # Create the combo box
        self.n = tk.StringVar()
        self.page = ttk.Combobox(self.control_frame, width = 27, textvariable = self.n)
        self.page.bind('<<ComboboxSelected>>', self.on_field_change)
        self.page['values'] = ('Page\\ 1')
        self.page.grid(column = 1, row = 5)
        self.page.current(0)

        # Create a button to save the pages as images and PDF file
        self.save_button = tk.Button(self.control_frame, text ="Save Output", command = self.saveCallBack)
        self.save_button.place(x=300,y=-5)

        self.label = ttk.Label(self.root, text = "")
        self.label.place(x=425,y=5)

        # Create the frame for the canvas, with a vertical scrollbar
        self.canvas_frame=tk.Frame(self.root)
        #self.canvas_frame.config(width=WIDTH*SIZE,height=500)
        self.canvas=tk.Canvas(
            self.canvas_frame,
            bg='#FFFFFF',
            scrollregion=(0,0,width*size,height*size)
        )

        self.vbar=tk.Scrollbar(
            self.canvas_frame,
            orient=tk.VERTICAL
        )

        # Create an image to draw the output on
        #white = 
        self.image = Image.new(
            "RGB",
            (width*output_size,height*output_size),
            color = (255, 255, 255) # type: ignore
        ) # type: ignore

        # Load the bitmap to draw the pixel
        self.pixel = Image.open('printers/printer_pixel.png')
        self.draw = ImageDraw.Draw(self.image)

        self.vbar.config(command=self.canvas.yview)
        self.canvas.config(width=width*size,height=height*size)
        self.canvas.config(yscrollcommand=self.vbar.set)

        #self.control_frame.pack()
        self.canvas_frame.pack(expand=True, fill=tk.BOTH)
        self.vbar.pack(side=tk.RIGHT,fill=tk.Y)
        self.canvas.pack(side=tk.LEFT,expand=True,fill=tk.BOTH)

        # Start at the top
        self.canvas.yview("moveto", 0.0)

        # Bind the up and down arrow to the canvas to scroll
        self.canvas.bind("<Up>",    lambda event: self.canvas.yview_scroll(-1, "units"))
        self.canvas.bind("<Down>",  lambda event: self.canvas.yview_scroll( 1, "units"))

        # First the canvas to be focused
        self.canvas.focus_set()
        self.canvas.bind("<1>", lambda event: self.canvas.focus_set())

        # Bind the resize callback
        self.canvas_frame.bind("<Configure>", self.resize)


    def create_ui(self):

        # Create root window
        self.select_printer(self.printer_selected)
        page_width = self.printer_profile.page_width
        page_height = self.printer_profile.page_height
        self.root = tk.Tk()
        self.root.title(self.printer_selected)
        self.root.geometry("{width}x{height}".format(width=(page_width*SIZE)+25,height=MIN_Y))
        self.root.resizable(False,True)
        self.root.minsize(500,MIN_Y)
        self.root.maxsize((page_width*SIZE)+25,2000)

        self.printer_select = tk.StringVar()
        self.printer_select.set(self.printer_selected)

        self.child_widgets(page_width, page_height, SIZE, OUTPUT_SIZE)

    # Clear the canvas and image
    def clear_canvas(self):
        self.canvas.delete("all")
        self.draw.rectangle([0, 0, self.printer_profile.page_width*OUTPUT_SIZE,self.printer_profile.page_height*OUTPUT_SIZE], fill='white', outline=None, width=1)

    # Redraw the currently selected page
    def redraw_page(self):

        # Clear the canvas
        self.clear_canvas()

        # Move scrollbar to the top of the page
        self.canvas.yview("moveto", 0.0)

        # Get the current page data
        pd = self.page_data[self.page_current]

        # Get the length of the page data
        pd_len = len(pd)

        # Set x and y to the top of the page
        self.printer_profile.x = pd[0]
        self.printer_profile.y = 0

        # Process all the data for the page
        for c in range(1,pd_len):
            i = pd[c]
            if self.printer_profile.graph_mode == False and i == FF:
                pass
            else:
                self.printer_profile.chout(i, add_data=False)


    # Create a new page
    def new_page(self):
        # Move to the top of the page
        self.canvas.yview("moveto", 0.0)

        # New page in the combobox
        values = self.page['values']
        current_values = list(values)
        self.page_current += 1
        self.page['values'] = current_values + ["Page {pg}".format(pg=self.page_current+1)]
        self.page.update()

        # Clear the canvas
        self.clear_canvas()

        # Add data array to the page
        self.page_data.append(bytearray())
        self.page_data[self.page_current].append(0)

        # Set the combobox to the new page
        self.page.current(self.page_current)

        # Set the x and y position to the top of the page
        self.printer_profile.x = 0
        self.printer_profile.y = 0

    # Draw a dot on the canvas and image
    def draw_dot(self, display_offset, output_offset):

        # Draw pixels for the display
        self.canvas.create_rectangle(
            self.printer_profile.x,
            self.printer_profile.y+display_offset,
            self.printer_profile.x+(SIZE * self.printer_profile.char_width),
            self.printer_profile.y+(SIZE)+display_offset,
            fill='black',
            outline=''
        )

        # Draw the pixels for the output
        Image.Image.paste(
            self.image,
            self.pixel,
            (int(self.printer_profile.x * OUTPUT_MULIPLIER), int(self.printer_profile.y * OUTPUT_MULIPLIER + output_offset))
        )

        # If it is double width, draw a second pixel
        if self.printer_profile.char_width != NORMAL_WIDTH:
            Image.Image.paste(
                self.image,
                self.pixel,
                (int(self.printer_profile.x * OUTPUT_MULIPLIER) + OUTPUT_MULIPLIER, int(self.printer_profile.y * OUTPUT_MULIPLIER + output_offset))
            )

        #self.draw.rectangle([self.printer_profile.x * OUTPUT_MULIPLIER, self.printer_profile.y * OUTPUT_MULIPLIER + output_offset, self.printer_profile.x * OUTPUT_MULIPLIER + (OUTPUT_SIZE * self.char_width)-1, self.printer_profile.y * OUTPUT_MULIPLIER +(OUTPUT_SIZE)+output_offset-1], fill='black', outline=None, width=1)

    # Output one vertical line of the character
    def output_byte(self, byte):
        display_offset = 0
        output_offset = 0
        for bit in range(0,self.printer_profile.font_height):
            bit_value = pow(2, bit)
            if (byte & bit_value == bit_value): self.draw_dot(display_offset, output_offset)

            display_offset += SIZE
            output_offset += OUTPUT_SIZE

    # Set the scroll position at the bottom of the output
    def set_scroll(self):
        # Get the width of the window frame
        w = self.canvas_frame.winfo_height()

        # Calculate the scrollbar percentage based on the printed line
        scroll =((self.printer_profile.y)-w)/((self.printer_profile.page_height*SIZE))

        # If the scrollbar percentage is greater than 100% set it to 100%
        # This should never happen
        if scroll > 1.0: scroll = 1.0

        # Set the scrollbar position
        self.canvas.yview("moveto", scroll)

    # Output a string
    def output_string(self, str):
        for i in str: self.printer_profile.chout(ord(i))
        self.canvas.update()

    # Callback for the the combo box change
    def on_field_change(self, event):
        # Get the current page and redraw it
        self.page_current = self.page.current()
        self.redraw_page()

    # Callback to save the output
    def saveCallBack(self):
        # Save the current page
        current_page = self.page_current

        # Iterate through the pages
        for i in range(0,len(self.page_data)):

            # Set the current page to the index and redraw, 
            # and save it
            self.page_current = i
            self.redraw_page()
            self.image.save('output_{page}.png'.format(page=i))

        # Restore the current page and redraw it
        self.page_current = current_page
        self.redraw_page()

        # Get a list of the image files that we produced
        files = []
        for i in range(0,len(self.page_data)):
            files.append('output_{page}.png'.format(page=i))

        # Load the images
        images = [
            Image.open("./" + f)
            for f in files
        ]

        # Save the images as PDF
        images[0].save(
            self.output_file,
            "PDF",
            resolution=100.0,
            save_all=True,
            append_images=images[1:]
        )

        # Delete the image files
        for f in files:
            os.unlink(f)

    # Callback for the frame being resized: Todo
    def resize(self, event):
        pass
        # if the current page is the last page
        #if self.page.current() == len(self.page_data)-1:
            # Set the scroll position
            #self.set_scroll()

    # Serial read task gets called from main loop
    def serial_read(self):
        try:
            # While there is something on the serial port
            while self.ser.inWaiting() > 0: # type: ignore

                # If the current page is not the last page
                if self.page.current() != len(self.page_data)-1:
                    # Set page to the last page to output to it
                    self.page.current(len(self.page_data)-1)
                    self.page_current = self.page.current()

                    # Redraw the page
                    self.redraw_page()

                # Send the character for output
                ch = self.ser.read(1)[0] # type: ignore
                self.printer_profile.chout(ch)

            # reschedule event in 20 milliseconds
            self.root.after(20, self.serial_read)
        except OSError:
            print("Serial port has been disconnected")
            self.serial.set("")
            self.serial_port = None
            self.refresh_menu()
            return

    # Get a block of text from from the line
    def get_block(self, line):

        # Start at 0
        cnt = 0

        # Find the start of the block or a comment
        while cnt < len(line) and (line[cnt] != '{' and line[cnt] != '#'):
            cnt += 1

        # We found a comment, return
        if cnt < len(line):
            if line[cnt] == '#': return ['','']

            # Get the line from the current position
            block = line[cnt:]

            # Get end of the block
            cnt = 0
            while cnt < len(block) and block[cnt] != '}':
                cnt += 1

            # Get the block
            block = block[:cnt+1]

            # Remove the block from the line
            line = line[cnt+1:]
        else:
            block = ''
            line = ''

        return [block, line]

    # Process the block
    def process_block(self, block):

        # Remove the end of block
        block = block[0:len(block)-1]

        # If string mode
        if block[0:2] == "{*":

            # Remove the start of block
            block = block[2:]

            # Print the output until the end of the block
            cnt = 0
            while  cnt < len(block):
                self.printer_profile.chout(ord(block[cnt]))
                cnt += 1

            return

        # If hex mode
        if block[0:2] == "{x":
            # Remove the start of the block
            block = block[2:]

            # Remove any spaces in the block
            block = block.replace(" ", "")

            # Make an even set up hex numbers
            if len(block) % 2 == 2: block += '0'

            # Output each of the hex numbers
            for i in range(0,len(block),2):
                value = int(block[i:i+2],16)
                self.printer_profile.chout(value)
            return

        # If decimal character mode
        if block[0] == '{':

            # Remove the start of the block
            block = block[ 1:]

            # Remove any spaces in the block
            block = block.replace(" ", "")

            # Split the values on comma
            ch_list = block.split(',')

            # Print the output
            for c in ch_list:
                self.printer_profile.chout(int(c))


    # Read the data file, and process each block
    def read_data_file(self, data_file):

        # Open the file and read the lines
        with open(data_file) as f:
            lines = f.read().splitlines()

        # for each line process the block
        for line in lines:

            # Remove leading and trailing spaces
            line = line.strip(' ')

            # While we have data in the line
            while len(line) > 0:

                # Get the block and the left over line
                result = self.get_block(line)
                block = result[0]
                line = result[1]

                # If we have a block process it
                if (len(block) > 0):
                    self.process_block(block)

    def donothing(self):
        pass

    # Open a display file and execute it
    def open_data_file(self):
        # Display dialog box with to get the file path
        file_path = filedialog.askopenfilename(
            title="Select a File To Display Output",
            filetypes=[("Text files", "*.txt"),
            ("All files", "*.*")]
        )

        # If we didn't cancel, read the file
        if file_path:
            self.read_data_file(file_path)


    # Save the output with a new name
    def save_as_output_to_pdf(self):

        # Display dialog to get the file to save
        f = filedialog.asksaveasfilename(
            defaultextension=".pdf",
            filetypes=[("Acrobat File","*.pdf"),("All Files","*.*")]
        )

        # If the dialog box with closed or canceled return
        if f is None: return

        # Set the output file to the selected file
        self.output_file = f

        # Save the output file
        self.saveCallBack()

        # Set the output file in the label
        self.label.config(text = self.output_file)

    # Save the output to a PDF file
    def save_output_to_pdf(self):
        # Save the output file
        self.saveCallBack()

    # Save printed data to a file
    def save_data_file(self):

        # Display dialog to get the file to save
        file = filedialog.asksaveasfilename(
            defaultextension=".txt",
            filetypes=[("All Files","*.*"),("Text Documents","*.txt")]
        )

        # If the dialog box is closed or canceled return
        if file is None: return

        # Set the output file to the selected file
        with open(file,"wt") as f:

            # Set mode to zero, since we don't know while kind of data we have yet
            mode = 0

            # Set data to empty string

            data = ""
            # Set the count to 0
            cnt = 0

            # Iterate through the page data
            for pd in self.page_data:

                # Iterate through the character data of the page data
                for ch in pd[1:]:

                    # If the mode is hex, and we have over 40 characters,
                    # write out the hex data, and clear the data
                    if mode == 2 and cnt > 40:
                        f.write("{x"+data+"}\n")
                        data = ""
                        cnt = 0

                    # If the mode is character mode, and we have over 40 characters
                    # write out the character data, and clear the data
                    if mode == 1 and cnt > 40:
                        f.write("{*"+data+"}\n")
                        data = ""
                        cnt = 0

                    # If we have a printable character
                    if (ch >= 32 and ch <= 91) or ch == 93 or (ch >= 97 and ch <= 121):
                        # If the mode is hex, write out the hex data, and clear the data
                        if mode == 2 and data != "":
                            f.write("{x"+data+"}\n")
                            data = ""
                            cnt = 0

                        # Add character to data and set mode to character
                        data += chr(ch)
                        mode = 1
                        cnt += 1
                    else:
                        # If the mode is character mode, write out the character data, and clear the data
                        if mode == 1 and data != "":
                            f.write("{*"+data+"}\n")
                            data = ""
                            cnt = 0

                        # Add a hex value to the data
                        data += '{:02X}'.format(ch)
                        mode = 2
                        cnt += 1

            # If we are done, output anything left in the data
            if mode == 1:
                f.write("{*"+data+"}")
            elif mode == 2:
                f.write("{x"+data+"}")

    # Open and read in binary printer data
    def open_binary_data_file(self):
        # Display dialog box with to get the file path
        file_path = filedialog.askopenfilename(
            title="Select a Hex file to load",
            defaultextension=".hex",
            filetypes=[("Hex files", "*.hex"),
            ("All files", "*.*")]
        )

        # If we didn't cancel, read the file
        if file_path:
            # Open the file and read the lines
            with open(file_path) as f:
                lines = f.read().splitlines()

            # for each line process the block
            for line in lines:

                # Remove leading and trailing spaces
                line = line.replace(' ','')

                for h in range(0,len(line),2):
                    c = int(line[h:h+2],16)
                    self.printer_profile.chout(c)



    # Save the print data as binary
    def save_binary_data_file(self):
        # Display dialog to get the file to save
        file = filedialog.asksaveasfilename(
            title="Select a file to save the hex",
            defaultextension=".hex",
            filetypes=[("Hex File","*.hex"),("All Files","*.*")]
        )

        # If the dialog box is closed or canceled return
        if file is None: return

        # Set the output file to the selected file
        with open(file,"wt") as f:
            # Set the count to 0

            cnt = 0
            # Iterate through the page data
            for pd in self.page_data:

                # Iterate through the character data of the page data
                for ch in pd[1:]:

                    # Write out the hex value
                    f.write("{:02x} ".format(ch))

                    # If we have 16 values, go to the next line,
                    # otherwise increment the counter
                    if cnt == 16:
                        f.write("\n")
                        cnt = 0
                    else:
                        cnt += 1

    # Clear the output from the pages
    def clear_output(self):
        # Clear the canvas
        self.clear_canvas()

        # Add buffer for the current page
        self.page_data = []
        self.page_data.append(bytearray())
        self.page_data[self.page_current].append(0)

        # Reset all the values
        self.printer_profile.x = 0
        self.printer_profile.y = 0
        self.page_current = 0
        self.x_last = 0
        self.y_last = 0
        self.page_last = 0

        self.printer_profile.clear_output()

        # Reset the combobox
        self.page['values'] = ('Page\\ 1')
        self.page.current(0)

    # Get the current serial ports
    def get_serial_ports(self):

        # Get the ports from the serial tool
        ports = serial.tools.list_ports.comports()

        # Initialize the port list
        p = []

        # Iterate through a sorted list of the ports
        for port, desc, hwid in sorted(ports):

            # If the description is not n/a, append the port
            if desc != "n/a":
                p.append([port, desc, hwid])

        # Return the serial port list
        return p

    # If the serial port has changed on the menu
    def serial_port_change(self):

        # If it is not the current port
        if self.serial_port != self.serial.get():

            # Close the old port
            if self.ser is not None:
                self.ser.close()

            # Get serial ports name
            self.serial_port = self.serial.get()

            # Open the port
            self.ser = serial.Serial(port=self.serial_port,baudrate=115200)
            print("Opened serial port",self.serial_port)

            # Set the time to read from the serial port
            self.root.after(20, self.serial_read)

    # Refrest the serial ports on the menu
    def refresh_menu(self):

        # Delete the items on the menu
        self.serial_menu.delete(0, 100)

        self.serial_menu.add_command(label="Refresh", command=self.refresh_menu)

        # Get the list of serial ports
        p = self.get_serial_ports()

        # If the we have at least one serial port
        if len(p) > 0:

            # Add a separator
            self.serial_menu.add_separator()

            # Populate the menu with the serial ports
            for i in p:
                self.serial_menu.add_radiobutton(label=i[1], value=i[0], variable=self.serial, command=self.serial_port_change)

        # If we do not have a serial port selected
        if self.serial_port is None:

            # If we only have one serial port
            if len(p) == 1:

                # Set the serial port
                self.serial_port = p[0][0]
                self.serial.set(self.serial_port)

                # Open that serial port
                self.ser = serial.Serial(port=self.serial_port,baudrate=115200)
                print("Opened serial port",self.serial_port)

                # Start the timer to read from the serial port
                self.root.after(20, self.serial_read)
            else:
                # If we have more than one serial port, to select any
                self.ser = None

    def set_printer(self):
        self.clear_output()
        self.select_printer(self.printer_select.get())
        self.refresh_ui()
        pass

    def create_menu(self):

        # Variable for the serial port
        self.serial = tk.StringVar()

        # Add the menu
        self.menubar = tk.Menu(self.root)

        # File menu
        self.filemenu = tk.Menu(self.menubar, tearoff=0)
        self.filemenu.add_command(label="Open Data File", command=self.open_data_file)
        self.filemenu.add_command(label="Save Data File", command=self.save_data_file)
        self.filemenu.add_separator()
        self.filemenu.add_command(label="Open Binary Data File", command=self.open_binary_data_file)
        self.filemenu.add_command(label="Save Binary Data File", command=self.save_binary_data_file)
        self.filemenu.add_separator()
        self.filemenu.add_command(label="Save As Output To PDF", command=self.save_as_output_to_pdf)
        self.filemenu.add_command(label="Save Output To PDF", command=self.save_output_to_pdf)
        self.filemenu.add_separator()
        self.filemenu.add_command(label="Clear Ourput", command=self.clear_output)
        self.filemenu.add_separator()
        self.filemenu.add_command(label="Exit", command=self.root.quit)
        self.menubar.add_cascade(label="File", menu=self.filemenu)

        # Printer menu
        self.printer_menu = tk.Menu(self.menubar, tearoff=0)
        #self.serial_menu.add_radiobutton(label=i[1], value=i[0], variable=self.serial, command=self.serial_port_change)

        self.printer_menu.add_radiobutton(label="MPS 801/1525", value="MPS 801", variable=self.printer_select, command=self.set_printer)
        self.printer_menu.add_radiobutton(label="MPS 802/1526", value="MPS 802", variable=self.printer_select, command=self.set_printer)
        self.printer_menu.add_radiobutton(label="VIC 1520", value="VIC 1520", variable=self.printer_select, command=self.set_printer)
        self.menubar.add_cascade(label="Printer", menu=self.printer_menu)

        # Serial menu
        self.serial_menu = tk.Menu(self.menubar, tearoff=0)

        # Get the serial ports
        s = self.get_serial_ports()

        # Add refresh submenu
        self.serial_menu.add_command(label="Refresh", command=self.refresh_menu)

        # If there are any serial ports
        if len(s) > 0:

            # Add separator before the ports
            self.serial_menu.add_separator()

            # Add the ports
            for i in s:
                self.serial_menu.add_radiobutton(label=i[1], value=i[0], variable=self.serial, command=self.serial_port_change)

        # Add the menu item
        self.menubar.add_cascade(label="Serial Port", menu=self.serial_menu)
        self.root.config(menu=self.menubar)

    def on_mousewheel(self, event):
        if platform.system() == "Darwin":
            # Process for MacOS
            self.canvas.yview_scroll(-1 * event.delta, 'units')
        elif platform.system() == "Windows":
            # Process for Windows
            self.canvas.yview_scroll(int(-1*(event.delta/120)), "units")
        else:
            # Process for Linux
            if event.num == 4:
                self.keyboard.press(Key.up)
                self.keyboard.release(Key.up)

            if event.num == 5:
                self.keyboard.press(Key.down)
                self.keyboard.release(Key.down)

    # Run the printer application
    def run(self, serial_port, data_file, output_file):
        # Create the menu
        self.create_menu()

        # Mouse wheel for MacOS
        if platform.system() == "Darwin":
            self.canvas.bind("<MouseWheel>", self.on_mousewheel)

        # Mouse wheel for Linux
        if platform.system() == "Windows":
            self.canvas.bind_all("<MouseWheel>", self.on_mousewheel)

        # Mouse wheel for Windows
        if platform.system() == "Linux":
            self.canvas.bind_all("<Button-4>", self.on_mousewheel)
            self.canvas.bind_all("<Button-5>", self.on_mousewheel)

        # Process the input file, if it is specified
        if data_file is not None:
            self.canvas.update()
            self.read_data_file(data_file)
            self.canvas.update()

        # Open the serial port for input
        self.serial_port = serial_port
        if self.serial_port is not None:
            self.ser = serial.Serial(port=serial_port,baudrate=115200)
            self.root.after(20, self.serial_read)
            self.serial.set(serial_port)
        else:
            p = self.get_serial_ports()
            if len(p) == 1:
                self.ser = serial.Serial(port=p[0][0],baudrate=115200)
                self.serial_port = p[0][0]
                self.serial.set(self.serial_port)
                print("Opened serial port",p[0][0])
                self.root.after(20, self.serial_read)
            else:
                self.ser = None

        self.output_file = "output.pdf"
        if output_file is not None:
            self.output_file = output_file

        # Set the output file to the label
        self.label.config(text = self.output_file)

        # Run the main loop
        self.root.mainloop()

        if self.ser is not None:
            self.ser.close()

def display_help():
    print ('printer.py [-s <serial port>] -f <data file>')
    sys.exit(2)

def main(argv):
    # Initialize the serial port, data file, and output file
    serial_port = None
    data_file = None
    output_file = None

    # Parse the command line argume
    # nts, and display the help if there is an error
    try:
        opts, args = getopt.getopt(argv,"hs:f:o:",["serial=","file=","--output"])
    except getopt.GetoptError:
        display_help()

    # Read the command line arguments
    for opt, arg in opts:
        if opt == '-h':
            display_help()
        elif opt in ("-s", "--serial"):
            serial_port = arg
        elif opt in ("-f", "--file"):
            data_file = arg
        elif opt in ("-o", "--output"):
            output_file = arg

    # If serial port specified and exists
    if serial_port is not None:
        if not os.path.exists(serial_port):
            print("Can not open port:", serial_port)
            exit(3)

    # If serial port specified and exists
    if data_file is not None:
        if not os.path.exists(data_file):
            print("The data file", data_file, "does not exist")
            exit(4)

    if output_file is not None:
        d = os.path.dirname(output_file)
        if d == '': d = "."
        if not os.path.exists(d):
            print("Directory for output file", d, "does not exist")
            exit(5)

    #printer_profile = mps802()
    #printer_profile = mps802()
    printer = Printer()
    #printer_profile.set_parent(printer)
    printer.run(serial_port, data_file, output_file)

if __name__ == "__main__":
    main(sys.argv[1:])
