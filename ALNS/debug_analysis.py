import csv
import re
import sys

def read_vehicles(path):
    vehs = []
    with open(path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            vehs.append(row['vehicle identifier'])
    return vehs

def parse_console_output(path, vehicle_ids):
    routes = {}
    
    try:
        with open(path, 'r', encoding='utf-16') as f:
            content = f.read()
    except:
        with open(path, 'r', encoding='utf-8') as f:
            content = f.read()
    lines = content.split('\n')
    
    for line in lines:
        line = line.strip()
        if line.startswith("Vehicle "):
            match = re.match(r"Vehicle (.*): (.*)", line)
            if match:
                v_id = match.group(1).strip()
                trace = match.group(2)
                
                if "Unused" in trace:
                    continue
                
                parts = trace.split(' -> ')
                events = []
                for p in parts:
                    if "(pickup)" in p:
                        eid = p.replace("(pickup)", "")
                        events.append(('pickup', eid))
                    elif "(drop)" in p:
                        eid = p.replace("(drop)", "")
                        events.append(('drop', eid))
                
                routes[v_id] = events
                
    return routes

def parse_csv_output(path):
    veh_data = {}
    with open(path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            vid = row['vehicle_id']
            eid = row['employee_id']
            ptime = row['pickup_time']
            dtime = row['drop_time']
            
            if vid not in veh_data:
                veh_data[vid] = []
            veh_data[vid].append({'eid': eid, 'ptime': ptime, 'dtime': dtime})
            
    return veh_data

def read_employees(path):
    emps = []
    with open(path, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            emps.append(row['User identifier'])
    return emps

def main():
    vehicle_file = "../Vehicle2.csv"
    employee_file = "../emp2.csv"
    output_txt = "final_output.txt" 

    csv_file = "output_vehicle.csv"

    print("Reading Inputs...")
    try:
        vehicle_ids = read_vehicles(vehicle_file)
        employee_ids = read_employees(employee_file)
    except Exception as e:
        print(f"Error reading inputs: {e}")
        return

    print("Parsing Console Output...")
    try:
        console_routes = parse_console_output(output_txt, vehicle_ids)
    except Exception as e:
        print(f"Error parsing console output: {e}")
        return

    print("Parsing CSV Output...")
    try:
        csv_routes = parse_csv_output(csv_file)
    except Exception as e:
        print(f"Error parsing CSV output: {e}")
        return

    print("\n--- COMPARISON ---")
    
    all_vehs = set(console_routes.keys()) | set(csv_routes.keys())
    
    for vid in sorted(all_vehs):
        c_events = console_routes.get(vid, [])
        csv_entries = csv_routes.get(vid, [])
        
        c_emps = []
        for type, eid in c_events:
            if type == 'pickup':
                try:
                    idx = int(eid)
                    if idx < len(employee_ids):
                        c_emps.append(employee_ids[idx])
                    else:
                        c_emps.append(f"UNKNOWN_ID_{eid}")
                except:
                    c_emps.append(eid)
        csv_emps = [x['eid'] for x in csv_entries]

        c_set = set(c_emps)
        csv_set = set(csv_emps)
        
        if c_set != csv_set:
            print(f"MISMATCH for {vid}:")
            print(f"  Console has: {sorted(list(c_set))}")
            print(f"  CSV has:     {sorted(list(csv_set))}")
            print(f"  Diff: {c_set ^ csv_set}")
        else:
            pass

    print("Comparison Done.")

if __name__ == "__main__":
    main()

