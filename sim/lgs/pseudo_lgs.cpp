ns3_time = 0
 
for sched in schedules:
  free_ops = sched.free_ops()
  for elem in free_ops:
    aq.push(elem)
 
 
while(!aq.empty())
    elem = aq.pop()
    elem.time = ns3_time;

if elem
    .op == comp
            : if nexto[host][proc] <=
              elem.time mark_started(elem) mexto[host][proc] = ns3_time + elem.size in_progress.push(elem) else
            : elem.time = nexto[host][proc] aq.push(elem); // reinsert

if elem.op == send:
     if local o,g avail:
       mark_started(elem)
       ns3_schedule(elem)
       in_progress.push(elem)
     else:
       elem.time = max(local o,g)
       aq.push(elem)
 
   if elem.op == recv:
     mark_started(elem)
     if (found a matching op_msg in local UQ):
       mark.done(elem)
     else:
      insert elem into RQ 
 
   if elem.op = msg:
     check for match in RQ, if found mark the send as done
     if not found, add elem to UQ
 
 
m = find minimum over inprogress ops len and aq
ns3_time = ns3_simulate_until(m, &op_msg)
for elem in inprogress:
  if elem.time + elem.size < ns3_time:
    set elem.host nexto to ns3_time
    mark_as_done(elem)
  if op_msg:
    add msg to aq
 
if (all RQ, UQ, aq are empty):
  ns3_terminate()