data = [
    #['AAAAAA' ,'ABC'],
    #['AAAAAA' ,'ABCDEFG'],
    #[chr(0xA0)+chr(0xA0)+chr(0xA0)+chr(0xA0)+'AAA  AAA    AAA','PETA'+chr(29)+'PET'+chr(29)+'PET'],
    #[chr(0xA0)+chr(0xA0)+chr(0xA0)+chr(0xA0)+'AAA  AAA    AAA','PETA'+chr(29)+'PET'],
    ['$$$$'   ,'99'   ,' $99'],
    ['$9999'  ,'99'   ,'$  99'],
    ['$99.99' ,'77'   ,'$77.00'],
    ['$99.99' ,'-77'  ,'$77.00'],
    ['$99.99-','-77'  ,'$77.00-'],
    ['$99.99-','77'   ,'$77.00'],
    ['S$99.99','77'   ,'+$77.00'],
    ['ZZZZ'   ,'77'   ,'0077'],
    ['ZZ.999' ,'77'   ,'77.000'],
    ['ZZZ.99','77'   ,'077.00'],
    ['999.99}','77'   ,'77.00'],
    ['.99'    ,'77'   ,'.**'],
    ['.99'    ,'.001' ,'.00'],
    ['S.999'  ,'.015' ,'+.015'],
    ['Z.999-' ,'.015' ,'-.015'],
    ['Z.999-' ,'-.015','0.015-']

]

"""
['$$$$'   ,'99'],
['$9999'  ,'99'],
['$99.99' ,'77'],
['$99.99' ,'-77'],
['$99.99-','-77'],
['$99.99-','77'],
['S$99.99','77'],
['ZZZZ'   ,'77'],
['ZZ.999' ,'77'],
['ZZZ.999','77'],
['999.99}','77'],
['.99'    ,'77'],
['.99'    ,'.001'],
['S.999'  ,'.015'],
['Z.999-' ,'.015'],
['Z.999-' ,'-.015']
"""

def sub_format(format, formatting):
    result = ''
    if formatting[0:1] in '1234567890.-':
        
        result = float(formatting)
        print()
    else:
        print(formatting)

    return result


def process_format(format, formatting):
    # Split format string into fields
    formatting_list = formatting.strip().split(chr(29))

    keep_chars = 'A9z$S.-'
    # Split string into fields with char 29
    format_string = ''
    format_list = []
    format_data = ''
    current_format = ''
    is_format = False

    # Separate the formatting from the text
    for f in format:
        if f not in keep_chars:
            if is_format:
                format_data += "~"
                is_format = False
                format_list.append(current_format)
                current_format = ''
            format_data += f
        else:
            current_format += f
            is_format = True

    # Process the last format
    if is_format:
        format_data += "~"
        is_format = False
        format_list.append(current_format)
        current_format = ''

    # Insert data into format

    format_len = len(format_list)
    formatting_len = len(formatting_list)
    for i in range(0,(format_len if format_len > formatting_len else formatting_len)):
        s = []
        if i < format_len and i < formatting_len:
            result = sub_format(format_list[i], formatting_list[i])
            #print(format_list[i],formatting_list[i])

        elif i < format_len and i >= formatting_len:
            print(format_list[i],"-")

        elif i >= format_len and i <= formatting_len:
            # Ignore formatting, since there is none
            pass

    print("----------------------------------------------------")


        #if i < format_len:
        #    s.append(format_list[i])
        #else:
        #    s.append('')

        #if i < formatting_len:
        #    s.append(formatting_list[i])
        #else:
        #    s.append('')

        #print(s)

    """
    if f in keep_chars:

        if f == chr(0x0a) or f == ' ':
            if space != True:
                formatting_list = formatting_list[1:]
            format_string += f
            space = True
        elif f == 'A':
            if len(formatting_list) > 0 and len(formatting_list[0]) > 0:
                format_string += formatting_list[0][0]
                formatting_list[0] = formatting_list[0][1:]
                space = False
            else:
                format_string += ' '
    """

    #print('format_string  :', format_string)
    #print("format_data    :",format_data)
    #print("format_list    :",format_list)
    #print("formatting_list:",formatting_list)



#for d in data:
#    process_format(d[0],d[1])

print("${:.0f}".format(-99.0))