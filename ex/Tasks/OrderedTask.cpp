/* Generated by Together */

#include "Tasks/OrderedTask.h"
#include "Tasks/TaskDijkstra.hpp"
#include "TaskPoints/FAISectorStartPoint.hpp"
#include "TaskPoints/FAISectorFinishPoint.hpp"
#include "TaskPoints/FAISectorASTPoint.hpp"
#include "TaskPoints/FAICylinderASTPoint.hpp"

void
OrderedTask::update_geometry() {

  task_projection.reset(tps[0]->getLocation());
  for (unsigned i=0; i<tps.size(); i++) {
    task_projection.scan_location(tps[i]->getLocation());
  }
  task_projection.report();

  for (unsigned i=0; i<tps.size(); i++) {
    tps[i]->update_geometry();
    tps[i]->clear_boundary_points();
    tps[i]->default_boundary_points();
    tps[i]->prune_boundary_points();
    tps[i]->update_projection();
  }
}

void OrderedTask::scan_distance(const GEOPOINT &location, bool full) 
{ 
  TaskDijkstra dijkstra(this);
  ScanTaskPoint start(0,0);

  SearchPoint ac(location, task_projection);

  ts->scan_active(tps[activeTaskPoint]);

  if (full) {
    distance_nominal = ts->scan_distance_nominal();

    // for max calculations, since one can still travel further in the sector,
    // we pretend we are on the previous turnpoint so the search samples will
    // contain the full boundary
    if (activeTaskPoint>0) {
      ts->scan_active(tps[activeTaskPoint-1]);
    }
    distance_max = dijkstra.distance_opt_achieved(ac, false);
  }

  ts->scan_active(tps[activeTaskPoint]);
  distance_min = dijkstra.distance_opt_achieved(ac, true);
  distance_remaining = ts->scan_distance_remaining(location);
  distance_travelled = ts->scan_distance_travelled(location);
  distance_scored = ts->scan_distance_scored(location);
  ts->scan_bearing_travelled(location);
  ts->scan_bearing_remaining(location);
}


void print_tp(OrderedTaskPoint *tp, std::ofstream& f) {
  unsigned n= tp->get_boundary_points().size();
  for (unsigned i=0; i<n; i++) {
    GEOPOINT loc = tp->get_boundary_points()[i].getLocation();
    f << loc.Longitude << " " << loc.Latitude << "\n";
  }
  f << "\n";
}

void print_sp(OrderedTaskPoint *tp, std::ofstream& f) {
  unsigned n= tp->get_search_points().size();
  for (unsigned i=0; i<n; i++) {
    GEOPOINT loc = tp->get_search_points()[i].getLocation();
    f << loc.Longitude << " " << loc.Latitude << "\n";
  }
  f << "\n";
}

extern int count_distance;


void OrderedTask::report(const GEOPOINT &location) 
{
/*
  d = dijkstra.distance_opt(start,true);
  printf("# absolute min dist %g\n",d);

  d = dijkstra.distance_opt(start,false);
  printf("# absolute max dist %g\n",d);
*/
  std::ofstream f1("res-task.txt");
  std::ofstream f2("res-max.txt");
  std::ofstream f3("res-min.txt");
  static std::ofstream f4("res-sample.txt");
  std::ofstream f5("res-ssample.txt");

  f1 << "#### Distances\n";
  f1 << "# dist nominal " << distance_nominal << "\n";
  f1 << "# min dist after achieving max " << distance_min << "\n";
  f1 << "# max dist after achieving max " << distance_max << "\n";
  f1 << "# dist remaining " << distance_remaining << "\n";
  f1 << "# dist travelled " << distance_travelled << "\n";
  f1 << "# dist scored " << distance_scored << "\n";

  f1 << "#### Task points\n";
  for (unsigned i=0; i<tps.size(); i++) {
    f1 << "## point " << i << "\n";
    print_tp(tps[i], f1);
    tps[i]->print(f1);
    f1 << "\n\n";
  }

  f5 << "#### Task sampled points\n";
  for (unsigned i=0; i<tps.size(); i++) {
    f5 << "## point " << i << "\n";
    print_sp(tps[i], f5);
  }

  f4 <<  location.Longitude << " " 
     <<  location.Latitude << "\n";

  f2 << "#### Max task\n";
  for (unsigned i=0; i<tps.size(); i++) {
    OrderedTaskPoint *tp = tps[i];
    f2 <<  tp->getMaxLocation().Longitude << " " 
       <<  tp->getMaxLocation().Latitude << "\n";
  }

  f3 << "#### Min task\n";
  for (unsigned i=0; i<tps.size(); i++) {
    OrderedTaskPoint *tp = tps[i];
    f3 <<  tp->getMinLocation().Longitude << " " 
       <<  tp->getMinLocation().Latitude << "\n";
  }

//  printf("distance tests %d\n", count_distance);
//  count_distance = 0;
}

bool OrderedTask::update_sample(const AIRCRAFT_STATE &state, 
                         const AIRCRAFT_STATE& state_last)
{
  ts->scan_active(tps[activeTaskPoint]);

  int n_task = tps.size();
  int t_min = std::max(0,(int)activeTaskPoint-1);
  int t_max = std::min(n_task-1, (int)activeTaskPoint+1);
  bool full_update = false;

  for (int i=t_min; i<=t_max; i++) {
    if (tps[i]->transition_enter(state, state_last)) {
    }
    if (tps[i]->transition_exit(state, state_last)) {
      if (i<n_task-1) {
        printf("transition to sector %d\n", i+1);
        setActiveTaskPoint(i+1);
        ts->scan_active(tps[activeTaskPoint]);
        // auto advance on exit for testing
      }
    }
    if (tps[i]->update_sample(state)) {
      full_update = true;
    }
  }
  scan_distance(state.Location, full_update);
  return true;
}

void
OrderedTask::remove(unsigned position)
{
  assert(position>0);
  assert(position<tps.size()-1);

  if (activeTaskPoint>position) {
    activeTaskPoint--;
  }

  delete legs[position-1]; 
  delete legs[position];   // 01,12,23,34 -> 01,34 
  legs.erase(legs.begin()+position-1); // 01,12,23,34 -> 01,23,34 
  legs.erase(legs.begin()+position-1); // 01,12,23,34 -> 01,34 

  delete tps[position];
  tps.erase(tps.begin()+position); // 0,1,2,3 -> 0,1,3

  legs.insert(legs.begin()+position-1,
              new TaskLeg(*tps[position-1],*tps[position])); // 01,13,34

  update_geometry();
}

void 
OrderedTask::insert(OrderedTaskPoint* new_tp, unsigned position)
{
  // remove legs first
  assert(position>0);

  if (activeTaskPoint>=position) {
    activeTaskPoint++;
  }

  delete legs[position-1];
  legs.erase(legs.begin()+position-1); // 0,1,2 -> 0,2

  tps.insert(tps.begin()+position, new_tp); // 0,1,2 -> 0,1,N,2

  legs.insert(legs.begin()+position-1,
              new TaskLeg(*tps[position-1],*tps[position])); // 0,1N,2

  legs.insert(legs.begin()+position,
              new TaskLeg(*tps[position],*tps[position+1])); // 0,1N,N2,2
  
  update_geometry();
}

void OrderedTask::setActiveTaskPoint(unsigned index)
{
  if (index<tps.size()) {
    activeTaskPoint = index;
  }
}

TaskPoint* OrderedTask::getActiveTaskPoint()
{
  if (activeTaskPoint<tps.size()) {
    return tps[activeTaskPoint];
  } else {
    return NULL;
  }
}

OrderedTask::~OrderedTask()
{
// TODO: delete legs and turnpoints
}

OrderedTask::OrderedTask()
{
  WAYPOINT wp[6];
  wp[0].Location.Longitude=0;
  wp[0].Location.Latitude=0;
  wp[1].Location.Longitude=0;
  wp[1].Location.Latitude=10;
  wp[2].Location.Longitude=10;
  wp[2].Location.Latitude=10;
  wp[3].Location.Longitude=7;
  wp[3].Location.Latitude=5;
  wp[4].Location.Longitude=10;
  wp[4].Location.Latitude=0;

  ts = new FAISectorStartPoint(task_projection,wp[0]);
  tps.push_back(ts);
  tps.push_back(new FAISectorASTPoint(task_projection,wp[1]));
  tps.push_back(new FAISectorASTPoint(task_projection,wp[2]));
  tps.push_back(new FAICylinderASTPoint(task_projection,wp[3]));
  tps.push_back(new FAISectorFinishPoint(task_projection,wp[4]));

  for (unsigned i=0; i+1<tps.size(); i++) {
    legs.push_back(new TaskLeg(*tps[i],*tps[i+1]));
  }

  update_geometry();
}
