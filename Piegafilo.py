
#
# Serial COM control program for 3d wire bender
# may/2019, Thomas Sosio
# 2 modules for this program: terminal.py and Serial.py.
#
import tkinter as tk
import tkinter.scrolledtext as tkscrolledtext
from tkinter import *
from tkinter import filedialog
import Serial
import serial.tools.list_ports
import _thread
import time
import webbrowser
from tkinter import messagebox
from tkinter import ttk
import sys
import glob


# globals
root = tk.Tk() # create a Tk root window
root.title( "Controllo Piegatrice" )

running = False
serialPort = Serial.SerialPort()
cmdFile = None
count = 0

baudrate = 9600
switch_variable = StringVar()
# set up the window size and position
root.state('zoomed')

root.grid_rowconfigure(4, weight=1)
root.grid_columnconfigure(0, weight=1)

frame0 = tk.Frame(root, bg='cyan', borderwidth= 5)
frame1 = tk.Frame(root, bg='cyan', borderwidth= 5)
frame2 = tk.Frame(root, bg='cyan', borderwidth= 5)
frame3 = tk.Frame(root, bg='cyan', borderwidth= 5)
frame4 = tk.Frame(root, bg='cyan', borderwidth= 5)
frame5 = tk.Frame(root, bg='cyan')


frame0.grid(row=0, sticky='nsew')
frame1.grid(row=1, sticky='nsew')
frame2.grid(row=2, sticky='nsew')
frame3.grid(row=3, sticky='nsew')
frame4.grid(row=4, sticky='nsew')
frame5.grid(row=5, sticky='nsew')

        
#frame2.grid_columnconfigure(1,weight=1)
#Create a nxm (rows x columns) grid inside the frame
for row_index in range(1):
    Grid.rowconfigure(frame0, row_index, weight=1)
    for col_index in range(5):
        Grid.columnconfigure(frame0, col_index, weight=1)
for row_index in range(1):
    Grid.rowconfigure(frame1, row_index, weight=1)
    for col_index in range(5):
        Grid.columnconfigure(frame1, col_index, weight=1)
for row_index in range(1):
    Grid.rowconfigure(frame2, row_index, weight=1)
    for col_index in range(4):
        Grid.columnconfigure(frame2, col_index, weight=1)
for row_index in range(1):
    Grid.rowconfigure(frame3, row_index, weight=1)
    for col_index in range(5):
        Grid.columnconfigure(frame3, col_index, weight=1)
Grid.rowconfigure(frame4, 0, weight=1)
Grid.columnconfigure(frame4, 0, weight=1)
Grid.rowconfigure(frame5, 0, weight=1)
Grid.columnconfigure(frame5, 0, weight=1)



# scrolled text box used to display the serial data
textbox = tkscrolledtext.ScrolledText(master=frame4, wrap='word') #width=characters, height=lines
textbox.grid(row=0,column=0,sticky='nsew')
textbox.config(font="bold")

#COM Port label
label_comport = Label(frame0,text="Porte disponibili (solo se libere):")
label_comport.grid(row=0,column=0,sticky='nse')
label_comport.config(font="bold", bg='cyan')

#COM Port discover and entry combobox
def serial_ports():
    if serialPort.IsOpen():
        result = []
        return result
    else:
        """ Lists serial port names

            :raises EnvironmentError:
                On unsupported or unknown platforms
            :returns:
                A list of the serial ports available on the system
        """
        if sys.platform.startswith('win'):
            ports = ['COM%s' % (i + 1) for i in range(256)]
        elif sys.platform.startswith('linux') or sys.platform.startswith('cygwin'):
            # this excludes your current terminal "/dev/tty"
            ports = glob.glob('/dev/tty[A-Za-z]*')
        elif sys.platform.startswith('darwin'):
            ports = glob.glob('/dev/tty.*')
        else:
            raise EnvironmentError('Unsupported platform')

        result = []
        for port in ports:
            try:
                s = serial.Serial(port)
                s.close()
                result.append(port)
            except (OSError, serial.SerialException):
                pass
        return result
if __name__ == '__main__':
    print(serial_ports())
comport_edit = ttk.Combobox(frame0, values=serial_ports())
comport_edit.grid(row=0,column=1,sticky='nsew')
comport_edit.config(font="bold")

# serial data callback function
def OnReceiveSerialData(message):
    str_message = message.decode("utf-8")
    textbox.insert('1.0', str_message)

# Register the callback above with the serial port object
serialPort.RegisterReceiveCallback(OnReceiveSerialData)


#
#  commands associated with button presses
#

#codice per disabilitare frame (pulsanti)
def normal(childList):
    for child in childList:
        child.configure(state="normal")
def disable(childList):
    for child in childList:
        child.configure(state="disable")
        
    
def OpenCommand():
    if button_openclose.cget("text") == 'Connetti':
        comport = comport_edit.get()
        print(comport)
        serialPort.Open(comport,baudrate)
        if serialPort.IsOpen():
            command=lambda: enable()
            button_openclose.config(text='Disconnetti',bg="green")
            textbox.insert('1.0', "Connesso\r\n")
    elif button_openclose.cget("text") == 'Disconnetti':
        if button_replaylog.cget('text') == 'Stop Replay Log':
            textbox.insert('1.0',"Stop Log Replay first\r\n")
        else:
            serialPort.Close()
            button_openclose.config(text='Connetti',bg="red")
            textbox.insert('1.0',"Disconnetti\r\n")


def ClearDataCommand():
    textbox.delete('1.0',END)

def SendDataCommand():
    message = senddata_edit.get()
    if serialPort.IsOpen():
        message += '\r\n'
        serialPort.Send(message)
        textbox.insert('1.0',message)
    else:
        textbox.insert('1.0', "Not sent - COM port is closed\r\n")

def SendDataCommand1(self):
    message = senddata_edit.get()
    if serialPort.IsOpen():
        message += '\r\n'
        serialPort.Send(message)
        textbox.insert('1.0',message)
    else:
        textbox.insert('1.0', "Not sent - COM port is closed\r\n")

def ReplayLogFile():
    try:
      if cmdFile != None:
        readline = cmdFile.readline()
        if not readline:
            print("no more lines")
        global serialPort
        serialPort.Send(readline)
        textbox.insert('1.0',readline)
    except:
      print("Exception in ReplayLogFile()")

def ReplayLogThread():
    while True:
        time.sleep(1.0)
        global cmdFile
        if serialPort.IsOpen():
            if cmdFile != None:
                ReplayLogFile()

def OpenFile():
    import os
    #Filename label
    label_file = Label(frame2)
    label_file.grid(row=0,column=1,sticky='nsw')
    label_file.config(font="bold", bg='cyan',text="FileName" )
    if 1:
        try:
            root.filename = filedialog.askopenfilename(initialdir="/", title="Seleziona file",
                                     filetypes=(("File CSV", "*.csv"), ("all files", "*.*")))
            global cmdFile
            cmdFile = open(root.filename)
            textbox.insert('1.0', "File selezionato: " + root.filename + "\r\n")
            button_replaylog.config(text='Nuovo file')
            nomeFile = root.filename
            label_file.configure(text = os.path.split(nomeFile) [1])
            label_file.update()
            normal(frame5.winfo_children())
        except:
            textbox.insert('1.0', "Impossibile aprire il file\r\n")

def send_commands():
    Npezzi = StringVar()
    global running
    global serialPort
    global count
    if running: # Only do this if the Stop button has not been clicked
        print ('quantita = ')
        Npezzi = qta_edit.get()
        if switch_variable.get() == "production":
            readline = cmdFile.readline()
            if not readline and count == (int(Npezzi)-1):
                running = False
                textbox.insert('1.0',"Fine ciclo\r\n")
                count = 0
                cmdFile.seek(0)
            elif not readline and count < (int(Npezzi)-1):
                cmdFile.seek(0)
                count += 1
                print('nuova parte')
                textbox.insert('1.0',"nuovo\r\n")
            serialPort.Send(readline)
            textbox.insert('1.0',readline)

        elif switch_variable.get() == "step":
            print("step")
            if not readline:
                print("no more lines")
                running = False
            serialPort.Send(readline)
            textbox.insert('1.0',readline)
            textbox.insert('1.0',"Fine ciclo")

        elif switch_variable.get() == "single":
            print("single")
            if not readline:
                print("no more lines")
                running = False
                textbox.insert('1.0',"Fine ciclo")
            serialPort.Send(readline)
            textbox.insert('1.0',readline)

        elif switch_variable.get() == "off":
            textbox.insert('1.0', "Seleziona modo produzione\r\n")
            print("modo non selezionato")
            running = False

    root.after(1, send_commands)

def Start():
    global running
    _thread.start_new_thread(send_commands, ())
    running = True




def DisplayAbout():
    tk.messagebox.showinfo(
    "About",
    "Written by Thomas Sosio \r\n\r\n" 
    "Custom serial commands sender \r\n\r\n" 
    "for 3D Wire bender \r\n"  
    "Source code at: Github URL: https://github.co\r\n")

def DisplayCommandsXXX():
    tk.messagebox.showinfo(
    "Comandi manuali disponibili",
    "'s' disabilita tutti motori\r\n" 
    "'r' restet controller\r\n\r\n" 
    "ASSE X, AVANZAMENTO FILO  \r\n"
    "'x',(numero tra -36760 e 32760)  avanzamento in centesimi di mm. \r\n" 
    "Esempi: x,500   X,5000  x,-80  X,-1100\r\n\r\n"
    "ASSE Y, ROTAZIONE TESTA\r\n" 
    "usare y minuscola per ruotare ad un angolo assoluto seguendo la via piu' breve, utilizzare Y maiuscola per poter impostare il senso di rotazione con segno negativo se necessario.\r\n" 
    "Esempi: y,500 (via più breve per 50 gradi)  Y,3000 (incrementare angolo fino a 300 gradi) Y,-400 (decrementare angolo fino a 40 gradi)  \r\n\r\n" 
    "cmd6  \r\n")

    
# COM Port open/close button
button_openclose = Button(frame0,text="Connetti",command=OpenCommand)
button_openclose.config(font="bold", bg="red")
button_openclose.grid(row=0,column=2,sticky='nsew')

#Toggle switches label
label_switch = Label(frame1,text="Seleziona modo:")
label_switch.grid(row=0,column=0,sticky='nse')
label_switch.config(font="bold", bg='cyan')

#Toggle step by step button
switch_variable = tk.StringVar(value="off")
production_button = tk.Radiobutton(master=frame1, text="PRODUZIONE", variable=switch_variable,
                            indicatoron=False, value="production", selectcolor="green", font="bold")
production_button.grid(row=0,column=1,sticky='nsew')
step_button = tk.Radiobutton(master=frame1, text="STEP BY STEP", variable=switch_variable,
                            indicatoron=False, value="step", selectcolor="green", font="bold")
step_button.grid(row=0,column=2,sticky='nsew')
single_button = tk.Radiobutton(master=frame1, text="SINGOLO", variable=switch_variable,
                            indicatoron=False, value="single", selectcolor="green", font="bold")
single_button.grid(row=0,column=3,sticky='nsew')


#Clear Rx Data button
#button_cleardata = Button(frame3,text="Clear Rx Data",command=ClearDataCommand)
#button_cleardata.config(font="bold")
#button_cleardata.grid(row=0,column=2,sticky='nsew')


#Seleziona il file
button_replaylog = Button(frame2,text="Seleziona file",command=OpenFile)
button_replaylog.config(font="bold")
button_replaylog.grid(row=0,column=0,sticky='nsew')

#Quantità label
label_qta = Label(frame2,text="Quantità: ")
label_qta.grid(row=0,column=2,sticky='nse',columnspan=2)
label_qta.config(font="bold", bg='cyan')
#Qantità entry box
def callback(self, P):
    if str.isdigit(P) or P == "":
        return True
    else:
        return False
qta_edit = Entry(frame2, validate = 'key', validatecommand = ((frame2.register(callback)), '%P', '%P'))
qta_edit.grid(row=0,column=4,sticky='nsew')
qta_edit.config(font="bold", justify='right')
qta_edit.insert(END,"")
  
#About button
button_about = Button(frame0,text="About",command=DisplayAbout)
button_about.config(font="bold")
button_about.grid(row=0,column=3,sticky='nsew')


#
# modo manuale
#

#COM Port label
label_manual = Label(frame3,text="Comando:")
label_manual.grid(row=0,column=0,sticky='nse')
label_manual.config(bg='cyan')

#Send Message button
button_senddata = Button(frame3,text="Invia comando manuale",command=SendDataCommand)
button_senddata.config()
button_senddata.grid(row=0,column=2,sticky='nsew')

#Send Data entry box
senddata_edit = Entry(frame3)
senddata_edit.grid(row=0,column=1,sticky='nsew')
senddata_edit.config()
senddata_edit.insert(END,"")
senddata_edit.bind('<Return>', SendDataCommand1)

#Send Message button
button_cmdlist = Button(frame3,text="Elenco comandi manuali", command=DisplayCommands)
button_cmdlist.config()
button_cmdlist.grid(row=0,column=3,sticky='nsew')

#
# start
#

#Start button
button_senddata = Button(frame5,text="Start", command=Start)
button_senddata.config(font="bold", bg='green', height=2)
button_senddata.grid(row=0,column=0,sticky='nsew')

def sdterm_main():
    root.after(2000, sdterm_main)  # run the main loop once each 200 ms
    #enable or disable buttons
    if serialPort.IsOpen():
        normal(frame1.winfo_children())
        normal(frame2.winfo_children())
        normal(frame3.winfo_children())
    else:
        disable(frame1.winfo_children())
        disable(frame2.winfo_children())
        disable(frame3.winfo_children())
        disable(frame5.winfo_children())
#
# The main loop
#        
root.after(0, sdterm_main)
root.mainloop()    
#

