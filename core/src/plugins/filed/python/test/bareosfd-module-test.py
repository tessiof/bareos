import bareosfd

def load_bareos_plugin(plugindef):
    print ("Hello from load_bareos_plugin")
    print (plugindef)
    print (bareosfd)
    bareosfd.DebugMessage(100, "Kuckuck")
    bareosfd.JobMessage(100, "Kuckuck")
