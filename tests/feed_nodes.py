import socket, struct, time, sys
MAGIC=0x534E4657; N=int(sys.argv[1]); socks=[]
for i in range(N):
    s=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); s.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1)
    s.bind((f"127.0.0.{2+i}",5005)); socks.append(s)
    a=socket.socket(socket.AF_INET,socket.SOCK_DGRAM); a.setsockopt(socket.SOL_SOCKET,socket.SO_REUSEADDR,1); a.bind((f"127.0.0.{2+i}",5006)); socks.append(a)
t0=time.time()
while time.time()-t0<float(sys.argv[2]) if len(sys.argv)>2 else 80:
    for i in range(N):
        s=socks[2*i]
        s.sendto(struct.pack("<IBBBBH6s",MAGIC,1,i,5,0,5006,bytes([0xaa,0xbb,0xcc,0xdd,0xee,i])),("127.0.0.1",5005))
        s.sendto(struct.pack("<IBB2xqiiiiIIIfI",MAGIC,4,i,1000+i*50,2500+i*300,300+i*40,960*(1+i%3),(-1)**i*(60+i*10),(i%3),(i+1)%2,0,(i-5)*3.0,[0,2,0,5,1,0,0,3,0,0][i]),("127.0.0.1",5005))
    for s in socks:
        s.setblocking(False)
        try:
            while True: s.recvfrom(2048)
        except BlockingIOError: pass
    time.sleep(1)
