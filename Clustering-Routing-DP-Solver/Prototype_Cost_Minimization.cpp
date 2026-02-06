#include <bits/stdc++.h>
using namespace std;
#define ll long long
#define int long long
#define pb push_back
using vpi = vector<pair<int, int>>;
using vi = vector<int>;
using vvi = vector<vector<int>>;
const double INF = 1e18;

class Node
{
    public:
    int x,y;
    double early;     
    double late; 
    double priority;
    bool is_drop;
};

double get_dist(const Node &a, const Node &b) 
{
    double dx = a.x - b.x;
    double dy = a.y - b.y;
    return sqrt(dx * dx + dy * dy);
}

int N, NODES,CAPACITY;
vector<vector<double>> dp;
vector<vector<double>> time_state;
vector<vector<int>> parent;

void solve(int mask, int k, int current_load, const vector<Node> &nodes)
{
    for (int i = 1; i < NODES; i++)
    {
        if (!(mask & (1 << i)))
        {
            double dist = get_dist(nodes[i], nodes[k]);
            int next_load = current_load;
            if (i <= N)
            { 
                if (current_load >= CAPACITY) continue; 
                    next_load++;
            } 
            else
            { 
                if (!(mask & (1 << (i - N)))) continue;
                    next_load--;
            }

            double arrival_time = dist + time_state[mask][k];
            double actual_time = arrival_time;
            if (!nodes[i].is_drop) 
                actual_time = max(arrival_time, nodes[i].early);

            if (actual_time > nodes[i].late) continue;

            int newMask = mask | (1 << i);
            double newCost = dp[mask][k] + dist;
            
            if (dp[newMask][i] > newCost)
            {
                dp[newMask][i] = newCost;
                time_state[newMask][i] = actual_time;
                parent[newMask][i] = k;
                solve(newMask, i, next_load, nodes);
            }
        }
    }
}


double timeToMinutes(string t) 
{
    int h = stoi(t.substr(0, 2));
    int m = stoi(t.substr(3, 2));
    return (double)h * 60 + m;
}

signed main()
{
    ios::sync_with_stdio(false);
    cin.tie(nullptr);
    cin>>N;
    if(N == 0) return 0;
    cin>>CAPACITY;

    NODES = 2 * N + 1;
    vector<Node> nodes(NODES);
    int sx, sy;
    cin >> sx >> sy;
    nodes[0] = {sx, sy, 0, 0, 0, false};
    for (int i = 1; i <= N; i++)
    {
        int px, py, dx, dy;
        string s_pick, s_drop;
        double priority;
        cin >> px >> py >> dx >> dy >> s_pick >> s_drop >> priority;
        double pick_min = timeToMinutes(s_pick);
        double drop_min = timeToMinutes(s_drop);
        nodes[i] = {px, py, pick_min, INF, priority, false}; 
        nodes[i + N] = {dx, dy, 0, drop_min, priority, true}; 
    }

    dp.assign(1 << NODES, vector<double>(NODES, INF));
    time_state.assign(1 << NODES, vector<double>(NODES, INF));
    parent.assign(1 << NODES, vector<int>(NODES, -1));

    dp[1][0] = 0;
    time_state[1][0] = 0;
    solve(1, 0, 0, nodes);

    double min_total_cost = INF;
    int endNode = -1;
    int finalMask = (1 << NODES) - 1;
    for (int i = 1; i < NODES; i++)
    {
        if (dp[finalMask][i] < min_total_cost)
        {
            min_total_cost = dp[finalMask][i];
            endNode = i;
        }
    }

    if (min_total_cost >= INF)
    {
        cout << "No valid path found." << endl;
    } 
    
    else 
    {
        vector<int> path;
        int currNode = endNode;
        int currMask = finalMask;
        while (currNode != -1) 
        {
            path.push_back(currNode);
            int prevNode = parent[currMask][currNode];
            
            currMask ^= (1 << currNode); 
            currNode = prevNode;
        }
        reverse(path.begin(), path.end());

        double running_cost = 0;
        for (size_t i = 0; i < path.size() - 1; i++)
        {
            int u = path[i];
            int v = path[i+1];
            running_cost += get_dist(nodes[u], nodes[v]);
        }
        cout <<"Minimum Cost: "<< fixed << setprecision(6) << running_cost << endl;
        
        cout << "Path Order: ";
        for (int i = 0; i < path.size(); i++) 
        {
            int idx = path[i];
            if (idx == 0) cout << "Start";

            else if (idx <= N) cout << "Pickup(" << idx << ")";
            else cout << "Drop(" << (idx - N) << ")";
            
            if (i != path.size() - 1) cout << " -> ";
        }
        cout << endl;
    }
    return 0;
}