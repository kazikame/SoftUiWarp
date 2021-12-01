import subprocess
import time
import re
import sys

# Performance test configuration.
iface = 'siw_enp94s0f0'
iterations = 10000
is_client = True
server_addr = '10.10.1.2'
max_msg_size_log2 = 16

# Base command to run.
base_cmd = '{binary} -d {iface} -n {iterations} -s {msg_size} -R'
if is_client:
    base_cmd += ' ' + server_addr

# Paths to different binaries.
perftest_path = '/users/sg99/perftest'
read_throughput_bin =   perftest_path+'/ib_read_bw'
read_latency_bin =      perftest_path+'/ib_read_lat'
write_throughput_bin =  perftest_path+'/ib_write_bw'
write_latency_bin =     perftest_path+'/ib_write_lat'
tests = {
    'Read Throughput':  read_throughput_bin,
    'Read Latency':     read_latency_bin,
    'Write Throughput': write_throughput_bin,
    'Write Latency':    write_latency_bin,
}

# Runs a command and returns stdout of the command.
def run_cmd(cmd):
    i = 0
    p = subprocess.run(cmd.split(' '), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
    while p.returncode != 0:
        stderr = p.stderr.decode('ascii')
        if "Couldn't connect" in stderr:
            if i < 5:
                # If can't connect to server, try waiting a couple times first.
                time.sleep(0.2*2**i)
            else:
                print('Timeout exceeded. Server doesn''t appear to be up!')
                sys.exit(0)
        else:
            # Unrecoverable error, print and quit.
            print('Error when running command "{}"'.format(cmd))
            print('Details below:\n')
            print(stderr)
            sys.exit(0)
        p = subprocess.run(cmd.split(' '), stdout=subprocess.PIPE, stderr=subprocess.PIPE)
        i += 1
    # Scrape data from the output.
    if is_client:
        result = p.stdout.decode('ascii')
        data_line = result.splitlines()[-2]
        data_line = data_line.strip()
        data_line = re.sub('\s+', ' ', data_line)
        return [float(x) for x in data_line.split()]

if is_client:
    print('Running performance tests as client with server {}.'.format(server_addr))
else:
    print('Running performance tests as server.')

# Throughput and latency over different message sizes.
for name, binary in tests.items():
    print('\nRunning test: "{}"'.format(name))
    for i in range(0, max_msg_size_log2+1):
        msg_size = 2 ** i
        cmd = base_cmd.format(binary=binary, iface=iface, iterations=iterations, msg_size=msg_size)
        stats = run_cmd(cmd)
        if is_client:
            print(' '.join([str(x) for x in stats]))
        else:
            print('Test {}/{} complete.\r'.format(i, max_msg_size_log2))
    
