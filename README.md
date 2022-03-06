# thermal-printer-print-translated-shopping-list
enter a shopping list, translates to target language item by item, prints to QR701 thermal printer
the printer connects to your network and serves a webpage on 192.168.4.1
the page contains javascript that sends entered english text to google translate for tranlation to a target language
the returned text is added to a canvas element, and thus stored as an image. your thermal printer need not support the
target language as the list is printed as an image.
click the print button and a websocket connection is opened to the printer and image data sent over several packets.
