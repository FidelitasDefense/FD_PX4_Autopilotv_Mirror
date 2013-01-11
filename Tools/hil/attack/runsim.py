#!/usr/bin/env python
# run a jsbsim model as a child process

import sys, os, pexpect, fdpexpect, socket
import math, time, select, struct, signal, errno
import random, numpy
import plotting as plot
import pickle
from collections import defaultdict

import attackDefinitions

sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', 'pysim'))
sys.path.insert(0, os.path.join(os.path.dirname(os.path.realpath(__file__)), '..', '..', '..', '..', 'mavlink', 'pymavlink'))

import util, fgFDM, atexit

from math import sin, cos

class control_state(object):
    def __init__(self):
        self.aileron = 0
        self.elevator = 0
        self.throttle = 0
        self.rudder = 0
        self.ground_height = 0

sitl_state = control_state()

def interpret_address(addrstr):
    '''interpret a IP:port string'''
    a = addrstr.split(':')
    a[1] = int(a[1])
    return tuple(a)

def jsb_set(variable, value):
    '''set a JSBSim variable'''
    global jsb_console
    jsb_console.send('set %s %s\r\n' % (variable, value))

def setup_template(home):
    '''setup aircraft/easystar/reset.xml'''
    v = home.split(',')
    if len(v) != 4:
        print("home should be lat,lng,alt,hdg - '%s'" % home)
        sys.exit(1)
    latitude = float(v[0])
    longitude = float(v[1])
    altitude = float(v[2])
    heading = float(v[3])
    sitl_state.ground_height = altitude
    template = os.path.join('aircraft', 'easystar', 'reset_template.xml')
    reset = os.path.join('aircraft', 'easystar', 'reset.xml')
    xml = open(template).read() % { 'LATITUDE'  : str(latitude),
                                    'LONGITUDE' : str(longitude),
                                    'HEADING'   : str(heading),
                                    'ALTITUDE'  : str(300+altitude)}
    open(reset, mode='w').write(xml)
    print("Wrote %s" % reset)

def process_sitl_input(buf):
    '''process control changes from SITL sim'''
    data = buf.rstrip().split('\t')
    if len(data) != 5:
        return
    aileron = float(data[0])
    elevator = float(data[1])
    rudder = float(data[2])
    running = bool(data[3])
    throttle = float(data[4])

    global wind
    #wind.speed      = speed*0.01
    #wind.direction  = direction*0.01
    #wind.turbulance = turbulance*0.01
    
    if aileron != sitl_state.aileron:
        jsb_set('fcs/aileron-cmd-norm', aileron)
        sitl_state.aileron = aileron
    if elevator != sitl_state.elevator:
        jsb_set('fcs/elevator-cmd-norm', elevator)
        sitl_state.elevator = elevator
    if rudder != sitl_state.rudder:
        jsb_set('fcs/rudder-cmd-norm', rudder)
        sitl_state.rudder = rudder
    if throttle != sitl_state.throttle:
        jsb_set('fcs/throttle-cmd-norm', throttle)
        sitl_state.throttle = throttle

def update_wind(wind):
    '''update wind simulation'''
    (speed, direction) = wind.current()
    jsb_set('atmosphere/psiw-rad', math.radians(direction))
    jsb_set('atmosphere/wind-mag-fps', speed/0.3048)
    

def process_jsb_input(buf):
    '''process FG FDM input from JSBSim'''
    global fdm, fg_out, sim_out
    fdm.parse(buf)
    if fg_out:
        try:
            agl = fdm.get('agl', units='meters')
            fdm.set('altitude', agl+sitl_state.ground_height, units='meters')
            fdm.set('rpm', sitl_state.throttle*1000)
            fg_out.send(fdm.pack())
        except socket.error as e:
            if e.errno not in [ errno.ECONNREFUSED ]:
                raise

    px4Format = "{}\t"*16 + "{}\n"

    # position
    lat = fdm.get('latitude', units='degrees')
    lon = fdm.get('longitude', units='degrees')
    alt = fdm.get('altitude', units='meters')

    # attitude
    phi = fdm.get('phi', units='radians')
    theta = fdm.get('theta', units='radians')
    psi = fdm.get('psi', units='radians')

    # test values
    #phi = 10*math.pi/180
    #theta = 20*math.pi/180
    #psi = -90*math.pi/180

    # rotation rates
    phidot = fdm.get('phidot', units='rps')
    thetadot = fdm.get('thetadot', units='rps')
    psidot = fdm.get('psidot', units='rps')

    p = phidot - psidot*sin(theta)
    q = cos(phi)*thetadot + sin(phi)*cos(theta)*psidot
    r = -sin(phi)*thetadot + cos(phi)*cos(theta)*psidot

    ran = random.gauss(0, rNoiseVar)
    #r = r + ran;

    # acceleration
    ax = fdm.get('A_X_pilot', units='mpss')
    ay = fdm.get('A_Y_pilot', units='mpss')
    az = fdm.get('A_Z_pilot', units='mpss')

    # test values 
    #ax = 0
    #ay = 0
    #az = -9.8
    #print "ax: {} ay: {} az: {}\n".format(ax,ay,az)

    # velocitiy
    vN = fdm.get('v_north', units='mps')
    vE = fdm.get('v_east', units='mps')
    vD = fdm.get('v_down', units='mps')
    vC = fdm.get('vcas', units='mps')
    #print "vN: {} vE: {} vD: {}\n".format(vN,vE,vD)

    simbuf = px4Format.format(
        time.clock(),
        lat, lon, alt,
        phi, theta, psi,
        p, q, r,
        ax, ay, az,
        vN, vE, vD, vC
        )
    try:
        sim_out.send(simbuf)
    except socket.error as e:
        if e.errno not in [ errno.ECONNREFUSED ]:
            raise

def check_flight_env(fdm):
    alt = fdm.get('altitude', units='meters')

    phi = fdm.get('phi', units='radians')
    theta = fdm.get('theta', units='radians')
    psi = fdm.get('psi', units='radians')

    phidot = fdm.get('phidot', units='rps')
    thetadot = fdm.get('thetadot', units='rps')
    psidot = fdm.get('psidot', units='rps')

    p = phidot - psidot*sin(theta)
    q = cos(phi)*thetadot + sin(phi)*cos(theta)*psidot
    r = -sin(phi)*thetadot + cos(phi)*cos(theta)*psidot

    if math.fabs(p) > math.pi or math.fabs(q) > math.pi or math.fabs(r) > math.pi:
        return False
    if alt <= 0:
        return False

    return True

def great_circle_dist(lat1, lon1, lat2, lon2):
    d2r = math.pi/180
    R = 6371000
    return math.acos(sin(lat1*d2r)*sin(lat2*d2r) + cos(lat1*d2r)*cos(lat2*d2r)*cos((lon2-lon1)*d2r)) * R;

def check_mission_env(fdm, tsim):

    alt_min = 2500
    alt_max = 3500
    
    lat0 = 37.616611
    lon0 = -122.416105

    destLat = 37.6082701666
    destLon = -122.400064179

    target_start_time = 70
    target_end_time = 80

    lat = fdm.get('latitude', units='degrees')
    lon = fdm.get('longitude', units='degrees')

    theater_size = 2000 
    target_window = 150

    theaterDist = great_circle_dist(lat0, lon0, lat, lon)
    targetDist = great_circle_dist(destLat, destLon, lat, lon)

    #print 'Theater distance = %f' % theaterDist
    #print 'Target distance = %f' % targetDist

    # Check that the vehicle is within the target window for the specified interval
    if tsim > target_start_time and tsim < target_end_time:
        #print 'Target distance = %f' % targetDist
        if targetDist > target_window:
            print 'Target window Violated'
            return False

    if theaterDist > theater_size:
        print 'Theater Window Violated'
        return False

    # Check that it is within the altitude range
    alt = fdm.get('altitude', units='meters')
    #if(alt < alt_min or alt > alt_max):
    #    return False;

    return True;

class Attack(object):
    def __init__(self,nominal,name,units,label,scripts,file_name,variable,attack_values,attack_comment):
        self.nominal = nominal
        self.name = name
        self.units = units
        self.label = label
        self.scripts = scripts
        self.file_name = file_name
        self.variable = variable
        self.attack_values = attack_values
        self.attack_comment = attack_comment

##################
# main program
from optparse import OptionParser
parser = OptionParser("runsim.py [options]")
parser.add_option("--simin",   help="SITL input (IP:port)",          default="127.0.0.1:5502")
parser.add_option("--simout",  help="SITL output (IP:port)",         default="127.0.0.1:5501")
parser.add_option("--fgout",   help="FG display output (IP:port)",   default="127.0.0.1:5503")
parser.add_option("--home",    type='string', help="home lat,lng,alt,hdg (required)")
parser.add_option("--script",  type='string', help='jsbsim model script', default='jsbsim/rascal_test.xml')
parser.add_option("--options", type='string', help='jsbsim startup options')
parser.add_option("--wind", dest="wind", help="Simulate wind (speed,direction,turbulance)", default='0,0,0')

(opts, args) = parser.parse_args()

for m in [ 'home', 'script' ]:
    if not opts.__dict__[m]:
        print("Missing required option '%s'" % m)
        parser.print_help()
        sys.exit(1)

os.chdir(util.reltopdir('Tools/hil'))

# kill off child when we exit
atexit.register(util.pexpect_close_all)

setup_template(opts.home)

latFreq = 0
rNoiseVar = 0

# start child
cmd = "JSBSim --realtime --suspend --nice --simulation-rate=1000 --logdirectivefile=jsbsim/fgout.xml --script=%s" % opts.script
if opts.options:
    cmd += ' %s' % opts.options

jsb = pexpect.spawn(cmd, logfile=sys.stdout, timeout=10)
jsb.delaybeforesend = 0
util.pexpect_autoclose(jsb)
i = jsb.expect(["Successfully bound to socket for input on port (\d+)",
                "Could not bind to socket for input"])
if i == 1:
    print("Failed to start JSBSim - is another copy running?")
    sys.exit(1)
jsb_out_address = interpret_address("127.0.0.1:%u" % int(jsb.match.group(1)))
jsb.expect("Creating UDP socket on port (\d+)")
jsb_in_address = interpret_address("127.0.0.1:%u" % int(jsb.match.group(1)))
jsb.expect("Successfully connected to socket for output")
jsb.expect("JSBSim Execution beginning")

# setup output to jsbsim
print("JSBSim console on %s" % str(jsb_out_address))
jsb_out = socket.socket(socket.AF_INET, socket.SOCK_STREAM)
jsb_out.connect(jsb_out_address)
jsb_console = fdpexpect.fdspawn(jsb_out.fileno(), logfile=sys.stdout)
jsb_console.delaybeforesend = 0

# setup input from jsbsim
print("JSBSim FG FDM input on %s" % str(jsb_in_address))
jsb_in = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
jsb_in.bind(jsb_in_address)
jsb_in.setblocking(0)

# socket addresses
sim_out_address = interpret_address(opts.simout)
sim_in_address  = interpret_address(opts.simin)

# setup input from SITL sim
sim_in = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sim_in.bind(sim_in_address)
sim_in.setblocking(0)

# setup output to SITL sim
sim_out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
sim_out.connect(interpret_address(opts.simout))
sim_out.setblocking(0)

# setup possible output to FlightGear for display
fg_out = None
if opts.fgout:
    fg_out = socket.socket(socket.AF_INET, socket.SOCK_DGRAM)
    fg_out.connect(interpret_address(opts.fgout))


# setup wind generator
wind = util.Wind(opts.wind)

fdm = fgFDM.fgFDM()

jsb_console.send('info\n')
jsb_console.send('resume\n')
jsb.expect("trim computation time")
time.sleep(1.5)
jsb_console.logfile = None

tstart = time.time()

print("Simulator ready to fly")

def reset_sim():
    jsb_set('simulation/reset',1)
    time.sleep(2)
    return time.time()

def main_loop():
    '''run main loop'''
    tnow = time.time()
    tFinal = 80
    last_report = tnow
    last_sim_input = tnow
    last_wind_update = tnow
    frame_count = 0
    input_count = 0
    paused = False

    # Load attack definitions and set the desired attack
    execfile('jsbsim/attackDefinitions.py', globals())
    print L
    attack1 = imuGyroNoise
    attack2 = gainAccelX

    global latFreq
    global rNoiseVar

    latFreq = attack1.nominal
    rNoiseVar = attack2.nominal

    resultKeys = ['flightFail', 'missionFail']
    innerSize = len(attack1.attack_values)
    outerSize = len(attack2.attack_values)

    results = dict.fromkeys(resultKeys)
    for key in results:
        results[key] = numpy.empty(shape=(innerSize, outerSize))
    
    for outerIndex, outerValue in enumerate(attack1.attack_values):
        iterResults = defaultdict(list)

        for innerIndex, innerValue in enumerate(attack2.attack_values):
            latFreq = attack1.attack_values[outerIndex]
            rNoiseVar = attack2.attack_values[innerIndex]

            # Reset the vehicle state
            tstart = reset_sim()

            tstart_sim = time.time() #fdm.get('time', units='seconds'),

            missionFailed = False

            while True:
                rin = [jsb_in.fileno(), sim_in.fileno(), jsb_console.fileno(), jsb.fileno()]
                try:
                    (rin, win, xin) = select.select(rin, [], [], 1.0)
                except select.error:
                    util.check_parent()
                    continue

                tnow = time.time()

                if jsb_in.fileno() in rin:
                    buf = jsb_in.recv(fdm.packet_size())
                    process_jsb_input(buf)
                    frame_count += 1

                if sim_in.fileno() in rin:
                    simbuf = sim_in.makefile().readline()
                    process_sitl_input(simbuf)
                    last_sim_input = tnow
                    input_count +=1

                # show any jsbsim console output
                if jsb_console.fileno() in rin:
                    util.pexpect_drain(jsb_console)
                if jsb.fileno() in rin:
                    util.pexpect_drain(jsb)

                #if tnow - last_sim_input > 0.5:
                    #if not paused:
                        #print("PAUSING SIMULATION")
                        #paused = True
                        #jsb_console.send('hold\n')
                #else:
                    #if paused:
                        #print("RESUMING SIMULATION")
                        #paused = False
                        #jsb_console.send('resume\n')

                # only simulate wind above 5 meters, to prevent crashes while
                # waiting for takeoff
                if tnow - last_wind_update > 0.1:
                    update_wind(wind)
                    last_wind_update = tnow

                if tnow - last_report > 3:
                    print("IPS %u FPS %u asl=%.1f agl=%.1f roll=%.1f pitch=%.1f a=(%.2f %.2f %.2f)" % (
                        input_count/(time.time() - last_report),
                        frame_count / (time.time() - last_report),
                        fdm.get('altitude', units='meters'),
                        fdm.get('agl', units='meters'),
                        fdm.get('phi', units='degrees'),
                        fdm.get('theta', units='degrees'),
                        fdm.get('A_X_pilot', units='mpss'),
                        fdm.get('A_Y_pilot', units='mpss'),
                        fdm.get('A_Z_pilot', units='mpss')))

                    print 'lat: %f lon: %f' % (fdm.get('latitude', units='degrees'),fdm.get('longitude', units='degrees'))
                    frame_count = 0
                    input_count = 0
                    last_report = time.time()

                # mission/ flight envelope check
                tsim = time.time()-tstart_sim #fdm.get('time', units='seconds') - tstart_sim,
                #if not missionFailed:
                    #if not check_mission_env(fdm, tsim):
                        #print 'Mission Envelope Failure!'
                        #missionFailed = True
                        #iterResults['missionFail'].append(tsim)

                #if not check_flight_env(fdm):
                    #print 'Flight Envelope Failure!'
                    #iterResults['flightFail'].append(tsim)
                    #if not missionFailed:
                        #iterResults['missionFail'].append(tsim)
                    #break

                if tsim > tFinal:
                    print 'Time has ended.'
                    if not missionFailed:
                        iterResults['missionFail'].append(tsim)
                    iterResults['flightFail'].append(tsim)
                    print 'lat: %f' % fdm.get('latitude', units='degrees'),
                    print 'lon: %f' % fdm.get('longitude', units='degrees'),
                    break

        for key in resultKeys:
            results[key][outerIndex] = iterResults[key]

    plotStructs = plot.generatePlotStructs(results, [attack1, attack2], os.getcwd()+'/')
    write = open('plotStructs.pkl', 'wb')
    pickle.dump(plotStructs, write)
    write.close()
    print plotStructs
    plot.generatePlots(plotStructs)


def exit_handler():
    '''exit the sim'''
    signal.signal(signal.SIGINT, signal.SIG_IGN)
    signal.signal(signal.SIGTERM, signal.SIG_IGN)
    # JSBSim really doesn't like to die ...
    if getattr(jsb, 'pid', None) is not None:
        os.kill(jsb.pid, signal.SIGKILL)
    jsb_console.send('quit\n')
    jsb.close(force=True)
    util.pexpect_close_all()
    sys.exit(1)

signal.signal(signal.SIGINT, exit_handler)
signal.signal(signal.SIGTERM, exit_handler)

try:
    main_loop()
except Exception, err:
    print "Exception: ", err
    exit_handler()
    raise
