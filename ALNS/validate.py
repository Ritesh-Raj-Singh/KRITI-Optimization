import csv
import sys
import re

def parse_time(mins):
    return float(mins)

def dist_km(x1, y1, x2, y2):
    return ((x1-x2)**2 + (y1-y2)**2)**0.5 * 111.0

def get_max_lateness(prio):
    if prio == 1: return 5
    if prio == 2: return 10
    if prio == 3: return 15
    if prio == 4: return 20
    return 30

def to_mins(t_str):
    h, m = map(int, t_str.split(':'))
    return h * 60 + m

def validate_solution(veh_file, emp_file, output_text):
    # Load Data
    vehs = {}
    with open(veh_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            vehs[row['vehicle identifier']] = row
    
    emps = {}
    with open(emp_file, 'r') as f:
        reader = csv.DictReader(f)
        for row in reader:
            emps[row['User identifier']] = row

    errors = []
    
    # Parse Output
    lines = output_text.split('\n')
    for line in lines:
        if not line.startswith('Vehicle '): continue
        parts = line.split(':')
        vid_raw = parts[0].replace('Vehicle ', '').strip()
        route_str = parts[1].strip()
        
        if route_str == "Unused" or route_str == "": continue
        
        # Vehicle Data
        v_idx = int(vid_raw)
        v_keys = list(vehs.keys())
        if v_idx >= len(v_keys): continue
        v_data = vehs[v_keys[v_idx]]
        
        # Simulation State
        curr_t = to_mins(v_data['vehicle availability time'])
        curr_x = float(v_data['current location x'])
        curr_y = float(v_data['current location y'])
        speed = float(v_data['average speed'])
        cap = int(v_data['seating capacity'])
        
        steps = [s.strip() for s in route_str.split('->')]
        if steps and steps[-1] == "End": steps.pop()
        
        batch = []
        
        # Need to map internal ID to CSV ID for employees
        e_keys = list(emps.keys())

        for step in steps:
            match = re.match(r'(\d+)\((pickup|drop)\)', step)
            if not match: continue
            
            eid = match.group(1)
            action = match.group(2)
            
            if int(eid) >= len(e_keys): 
                print(f"Error: Emp ID {eid} out of bounds")
                continue
            
            e_data = emps[e_keys[int(eid)]]
            
            e_x = float(e_data['Pick-up Location X'])
            e_y = float(e_data['Pick-up Location Y'])
            
            if action == "pickup":
                # Travel
                d = dist_km(curr_x, curr_y, e_x, e_y)
                travel_t = (d / speed) * 60.0
                curr_t += travel_t
                
                # Wait if early
                ready = to_mins(e_data['Available time window start'])
                curr_t = max(curr_t, ready)
                
                # Service
                curr_t += 1.0
                curr_x = e_x
                curr_y = e_y
                
                batch.append(eid)
                
                if len(batch) > cap:
                    errors.append(f"Capacity violated at V{v_idx} for Emp {eid}. Load {len(batch)} > {cap}")

                # Premium Check
                wants_premium = (e_data.get('Vehicle Type Preference', '').lower() == 'premium')
                is_premium = (v_data.get('Category / Type', '').lower() == 'premium')
                
                if wants_premium and not is_premium:
                     errors.append(f"Premium Violation: Emp {eid} wants premium but assigned to V{v_idx} ({v_data.get('Category / Type', 'Unknown')})")

                # Share Pref check
                pref = e_data['Sharing preference']
                limit = 100
                if pref == 'single': limit = 1
                elif pref == 'double': limit = 2
                elif pref == 'triple': limit = 3
                
                if len(batch) > limit:
                     errors.append(f"Share Pref violated at V{v_idx} for Emp {eid} ({pref}). Load {len(batch)}")
                
                # Check others share pref
                for pid in batch:
                    p_data = emps[e_keys[int(pid)]]
                    p_pref = p_data['Sharing preference']
                    p_limit = 100
                    if p_pref == 'single': p_limit = 1
                    elif p_pref == 'double': p_limit = 2
                    elif p_pref == 'triple': p_limit = 3
                    
                    if len(batch) > p_limit:
                        errors.append(f"Share Pref violated for PASSENGER {pid} ({p_pref}) when adding {eid}. Load {len(batch)}")
                    
            elif action == "drop":
                dest_x = float(e_data['Destination Location X'])
                dest_y = float(e_data['Destination Location Y'])
                
                d_off = dist_km(curr_x, curr_y, dest_x, dest_y)
                if d_off > 0.01:
                    travel_t = (d_off / speed) * 60.0
                    curr_t += travel_t
                    curr_x = dest_x
                    curr_y = dest_y
                
                # Check Lateness
                due = to_mins(e_data['Available time window end'])
                prio = int(e_data['User priority / level'])
                limit_time = due + get_max_lateness(prio)
                
                if curr_t > limit_time:
                     errors.append(f"Lateness violated: V{v_idx} Emp {eid}. Arr {curr_t:.1f} > Limit {limit_time} (Due {due}, Prio {prio}, Buffer {get_max_lateness(prio)})")

                if eid in batch:
                    batch.remove(eid)

    if not errors:
        print("All Validation Checks Passed!")
    else:
        print("Found Violations:")
        for e in errors:
            print(e)

if __name__ == "__main__":
    with open('output.txt', 'r') as f:
        content = f.read()
    validate_solution('../Vehicle2.csv', '../emp2.csv', content)
