import time
import threading
import wx
import can
import can.interfaces.ixxat

DESTIDSHIFT = 0
CMDSHIFT = 8
CPUEDGESHIFT = 16
FORMATSHIFT = 18
CMDCATSHIFT = 20
TYPESHIFT = 24

logs = ""
MAP_ON = False
modules = {}
lookup = [["","(Config Aware","T{:x}E{:x}i{:x}Q{:x} (Query","T{:x}E{:x}i{:x}S{:x} (Set ID","T{:x}E{:x}i{:x}R{:x} (Set Relay"],
          ["T{:x}E{:x}i{:x}C{:x}{:x}N{:x}{:x} (TopoMUX","T{:x}E{:x}i{:x}P{:x} (TopoPower Cold","T{:x}E{:x}i{:x}H{:x} (TopoPower Hot","T{:x}E{:x}i{:x}t{:x}B{:x}l{:x}r{:x} (Power Status"],
          ["T{:x}E{:x}i{:x}I{:x}o{:x} (IO Command"]]
configs = can.detect_available_configs("ixxat")
for config in configs:
    print(config)

bus = can.interfaces.ixxat.IXXATBus(channel=0,bitrate=1000000)

class Window(wx.Frame): 
    def __init__(self,title):
        super().__init__(parent=None, title=title, style=wx.DEFAULT_FRAME_STYLE)
        self.SetSize(800, 750)
        self.panel = wx.Panel(self)

        self.labelID = wx.StaticText(self.panel,label="Identifier",pos=(50,30))
        self.labelID.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.id    = wx.TextCtrl(self.panel, value = '00000000',pos=(50, 50), size=(180, 50),style=wx.ALIGN_RIGHT)
        self.id.SetFont(wx.Font(25, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        self.labelbyte1 = wx.StaticText(self.panel,label="Byte 1",pos=(230,30))
        self.labelbyte1.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.byte1 = wx.TextCtrl(self.panel, value = '00',pos=(230, 50), size=(50, 50),style=wx.ALIGN_RIGHT)
        self.byte1.SetFont(wx.Font(25, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        self.labelbyte2 = wx.StaticText(self.panel,label="Byte 2",pos=(280,30))
        self.labelbyte2.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.byte2 = wx.TextCtrl(self.panel, value = '00',pos=(280, 50), size=(50, 50),style=wx.ALIGN_RIGHT)
        self.byte2.SetFont(wx.Font(25, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        self.labelbyte3 = wx.StaticText(self.panel,label="Byte 3",pos=(330,30))
        self.labelbyte3.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.byte3 = wx.TextCtrl(self.panel, value = '00',pos=(330, 50), size=(50, 50),style=wx.ALIGN_RIGHT)
        self.byte3.SetFont(wx.Font(25, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        self.labelbyte4 = wx.StaticText(self.panel,label="Byte 4",pos=(380,30))
        self.labelbyte4.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.byte4 = wx.TextCtrl(self.panel, value = '00',pos=(380, 50), size=(50, 50),style=wx.ALIGN_RIGHT)
        self.byte4.SetFont(wx.Font(25, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        self.labelbyte5 = wx.StaticText(self.panel,label="Byte 5",pos=(430,30))
        self.labelbyte5.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.byte5 = wx.TextCtrl(self.panel, value = '00',pos=(430, 50), size=(50, 50),style=wx.ALIGN_RIGHT)
        self.byte5.SetFont(wx.Font(25, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        self.labelbyte6 = wx.StaticText(self.panel,label="Byte 6",pos=(480,30))
        self.labelbyte6.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.byte6 = wx.TextCtrl(self.panel, value = '00',pos=(480, 50), size=(50, 50),style=wx.ALIGN_RIGHT)
        self.byte6.SetFont(wx.Font(25, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        self.labelbyte7 = wx.StaticText(self.panel,label="Byte 7",pos=(530,30))
        self.labelbyte7.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.byte7 = wx.TextCtrl(self.panel, value = '00',pos=(530, 50), size=(50, 50),style=wx.ALIGN_RIGHT)
        self.byte7.SetFont(wx.Font(25, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        self.labelbyte8 = wx.StaticText(self.panel,label="Byte 8",pos=(580,30))
        self.labelbyte8.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.byte8 = wx.TextCtrl(self.panel, value = '00',pos=(580, 50), size=(50, 50),style=wx.ALIGN_RIGHT)
        self.byte8.SetFont(wx.Font(25, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        send_button = wx.Button(self.panel, label="SEND", pos=(650, 50), size=(100, 50))
        send_button.SetFont(wx.Font(15, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        send_button.Bind(wx.EVT_BUTTON,self.SEND)

        self.error = wx.StaticText(self.panel, label = "",pos=(50,120))
        self.error.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        self.map = wx.TextCtrl(self.panel,style=wx.TE_MULTILINE | wx.TE_READONLY | wx.VSCROLL,pos=(50,150),size=(700,450))
        self.map.SetFont(wx.Font(13, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.map.Hide()

        self.show_map = wx.Button(self.panel, label="LOGS", pos=(0, 0), size=(50, 25))
        self.show_map.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        self.show_map.Bind(wx.EVT_BUTTON,self.MAP)

        self.logs = wx.TextCtrl(self.panel,style=wx.TE_MULTILINE | wx.TE_READONLY | wx.VSCROLL,pos=(50,150),size=(700,450))
        self.logs.SetFont(wx.Font(10, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
    
        self.filelocation = wx.TextCtrl(self.panel, value = 'C:\\',pos=(180, 620), size=(440,25),style=wx.ALIGN_LEFT)

        save_button = wx.Button(self.panel,label="SAVE",pos=(50,620) ,size=(100,50))
        save_button.SetFont(wx.Font(15, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        save_button.Bind(wx.EVT_BUTTON,self.SAVE)

        clear_button = wx.Button(self.panel,label="CLEAR",pos=(650,620) ,size=(100,50))
        clear_button.SetFont(wx.Font(15, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        clear_button.Bind(wx.EVT_BUTTON,self.CLEAR)

        self.saveerror = wx.StaticText(self.panel, label = "",pos=(180,650))
        self.saveerror.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))

        self.Show()
    
    def SEND(self,_):
        global lookup
        try:
            id = int(self.id.GetValue(),16)
            if id > 0x1fffffff: raise
            data1 = int(self.byte1.GetValue(),16)
            data2 = int(self.byte2.GetValue(),16)
            data3 = int(self.byte3.GetValue(),16)
            data4 = int(self.byte4.GetValue(),16)
            data5 = int(self.byte5.GetValue(),16)
            data6 = int(self.byte6.GetValue(),16)
            data7 = int(self.byte7.GetValue(),16)
            data8 = int(self.byte8.GetValue(),16)
            data = [data1, data2, data3, data4, data5, data6, data7, data8]
            tosend = can.Message(arbitration_id=id, data=data, is_extended_id=True if id > 4095 else False, timestamp=time.time())
            bus.send(tosend)
            cmd    = (id>>CMDSHIFT)&0xFF
            cmdcat = (id>>CMDCATSHIFT)&0xF
            format = (id>>FORMATSHIFT)&0x3
            t = (id>>TYPESHIFT)
            i      = id&0xFF
            e      = (id>>CPUEDGESHIFT)&0x3
            d = [0]*16
            if format == 0:
                for j in range(8):
                    d[j] = data[j]
            elif format == 1:
                for k in range(8):
                    d[2*k] = data[k]>>4
                    d[2*k+1] = data[k]&0xF
            print(hex(id))
            self.logs.AppendText(f"{time.strftime('%H:%M:%S', time.localtime(tosend.timestamp))} Sent:     {hex(tosend.arbitration_id).zfill(10)} 0x{tosend.data.hex()} {lookup[cmdcat%8][cmd].format(t,e+1,i,d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10],d[11],d[12],d[13],d[14],d[15]) if lookup[cmdcat%8][cmd]!= "" else "(Unknown Command"}{" Reply" if cmdcat >=8 else ""})\n")
            self.error.SetLabel("SENT!")
            self.error.SetBackgroundColour(wx.GREEN)
            self.error.SetForegroundColour(wx.BLACK)
            self.error.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
        except:
            self.error.SetLabel("ERROR: Invalid data frame!")
            self.error.SetBackgroundColour(wx.RED)
            self.error.SetForegroundColour(wx.WHITE)
    
    def SAVE(self,_):
        file_path = self.filelocation.GetValue()
        file_path2 = self.filelocation.GetValue()
        file_path += "\\logs.txt"
        file_path2 += "\\map.txt"

        try:
            with open(file_path, "w") as file:
                file.write(self.logs.GetValue())
            with open(file_path2,"w") as file:
                file.write(self.map.GetValue())
            self.saveerror.SetLabel(f"Successfully saved to {file_path}")
            self.saveerror.SetBackgroundColour(wx.GREEN)
            self.saveerror.SetForegroundColour(wx.BLACK)
        except IOError as e:
            self.saveerror.SetLabel(f"Error saving file: {e}")
            self.saveerror.SetBackgroundColour(wx.RED)
            self.saveerror.SetForegroundColour(wx.WHITE)

    def READ(self):
            self.logs.AppendText(logs)
    
    def CLEAR(self,_):
        self.logs.SetValue("")

    def MAP(self,_):
        global MAP_ON
        global modules
        if MAP_ON:
            self.show_map.SetLabel("LOGS")
            self.error.SetFont(wx.Font(9, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
            self.error.SetLabel("")
            self.logs.Show()
            self.map.Hide()
        else:
            self.show_map.SetLabel("MAP")
            self.map.SetLabelText("")
            dir = "TBLR"
            self.error.SetFont(wx.Font(13, family = wx.FONTFAMILY_MODERN, style=0, weight=90))
            self.error.SetLabel(f"{"ID":^6}{"TYPE":^6}{"TOP":^6}{"FACE":^6}{"RIGHT":^6}{"FACE":^6}{"BOTTOM":^6}{"FACE":^6}{"LEFT":^6}{"FACE":^6}")
            self.error.SetBackgroundColour(wx.WHITE)
            self.error.SetForegroundColour(wx.BLACK)
            for module in modules.keys():
                topneighbourid = int(modules[module][1][:2],16)
                topneighbourface = int(modules[module][1][8:10],16)
                if topneighbourid == 99: topneighbourid = '-'

                rightneighbourid = int(modules[module][1][2:4],16)
                if rightneighbourid == 99: rightneighbourid = '-'
                rightneighbourface = int(modules[module][1][10:12],16)

                bottomneighbourid = int(modules[module][1][4:6],16)
                if bottomneighbourid == 99: bottomneighbourid = '-'
                bottomneighbourface = int(modules[module][1][12:14],16)

                leftneighbourid = int(modules[module][1][6:8],16)
                if leftneighbourid == 99: leftneighbourid = '-'
                leftneighbourface = int(modules[module][1][14:16],16)

                self.map.AppendText(f"{module:^5}|{modules[module][0]:^5}|")
                self.map.AppendText(f"{topneighbourid:^5}|{dir[topneighbourface] if topneighbourid!='-' else '-':^5}|")
                self.map.AppendText(f"{rightneighbourid:^5}|{dir[rightneighbourface] if rightneighbourid!='-' else '-':^5}|")
                self.map.AppendText(f"{bottomneighbourid:^5}|{dir[bottomneighbourface] if bottomneighbourid!='-' else '-':^5}|")
                self.map.AppendText(f"{leftneighbourid:^5}|{dir[leftneighbourface] if leftneighbourid!='-' else '-':^5}\n")
            self.map.Show()
            self.logs.Hide()
        MAP_ON = not MAP_ON

def task():
    global logs
    global modules
    global lookup
    while True:
        msg = bus.recv(1)
        if msg is not None:
            cmd    = (msg.arbitration_id>>CMDSHIFT)&0xFF
            cmdcat   = (msg.arbitration_id>>CMDCATSHIFT)&0xF
            i      = msg.arbitration_id&0xFF
            e      = (msg.arbitration_id>>CPUEDGESHIFT)&0x3
            t      = (msg.arbitration_id>>TYPESHIFT)
            format = (msg.arbitration_id>>FORMATSHIFT)&0x3
            data = list(msg.data)
            d = [0,0,0,0,0,0,0,0,0,0,0,0,0,0,0,0]
            if format == 0:
                for j in range(8):
                    d[j] = data[j]
            elif format == 1:
                for k in range(8): 
                    d[2*k] = data[k]>>4
                    d[2*k+1] = data[k]&0xF
            logs = f"{time.strftime('%H:%M:%S', time.localtime(msg.timestamp))} Received: {hex(msg.arbitration_id).zfill(10)} 0x{msg.data.hex()} {lookup[cmdcat%8][cmd].format(t,e+1,i,d[0],d[1],d[2],d[3],d[4],d[5],d[6],d[7],d[8],d[9],d[10],d[11],d[12],d[13],d[14],d[15]) if lookup[cmdcat%8][cmd]!= "" else "Unknown Command"}{" Reply" if cmdcat >=8 else ""})\n"
            if cmd==1 and cmdcat == 0:
                if i not in modules.keys():
                    modules.update({i:[t,msg.data.hex()]})
                else:
                    modules[i] = [t,msg.data.hex()]
            window.READ()

app = wx.App()
window = Window("CAN Logger")
threading.Thread(target=task, daemon=True).start()
app.MainLoop()    