"use client";

import React from 'react';
import {
  MapPin, TrendingDown, Clock,
  ShieldCheck, DollarSign, Timer, Truck, AlertTriangle, Star
} from 'lucide-react';
import { BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, ResponsiveContainer, PieChart, Pie, Cell } from 'recharts';

const formatValue = (value) => {
  if (typeof value === 'number') return value.toFixed(1);
  return value;
};

const PieTooltip = ({ active, payload }) => {
  if (active && payload && payload.length) {
    const data = payload[0].payload;
    return (
      <div className="bg-[#1F2937] border border-gray-700 rounded-lg px-3 py-2 text-xs shadow-lg">
        <p className="text-gray-300 font-mono">{data.name}</p>
        <p className="text-amber-400 font-bold">{data.value} vehicles ({((data.value / (data.value + (payload[0].payload.otherValue || 0)) * 100).toFixed(1))}%)</p>
      </div>
    );
  }
  return null;
};

export default function VeloraAnalytics({ data }) {
  const analytics = data || {
    totalEmployees: 0,
    totalVehiclesUsed: 0,
    totalVehiclesAvailable: 0,
    totalDistance: "0.0",
    totalScore: 0,
    totalOptimizedCost: "0",
    totalBaselineCost: "0",
    totalOptimizedTime: 0,
    totalBaselineTime: 0,
    costSavings: "0",
    timeSavings: "0",
    perVehicleCostData: [],
    employeeTimeComparison: [],
    unassignedVehicleIds: [],
    violations: [],
    compliance: [
      { label: 'Vehicle Preference', current: 0, max: 0, percent: 0 },
      { label: 'Sharing Preference', current: 0, max: 0, percent: 0 },
      { label: 'Time Windows', current: 0, max: 0, percent: 0 }
    ]
  };

  const hasRealData =
    (data && (
      analytics.totalEmployees > 0 ||
      analytics.perVehicleCostData.length > 0 ||
      analytics.employeeTimeComparison.length > 0
    ));

  if (!hasRealData) {
    return (
      <div className="min-h-screen bg-[#0C0E14] flex flex-col items-center justify-center">
        <div className="relative">
          <div className="w-16 h-16 rounded-full border-4 border-gray-800"></div>
          <div className="absolute top-0 left-0 w-16 h-16 rounded-full border-4 border-t-cyan-400 border-r-transparent border-b-transparent border-l-transparent animate-spin"></div>
        </div>
        <p className="mt-6 text-gray-400 font-mono text-sm tracking-wider animate-pulse">
          FETCHING ANALYTICS DATA
        </p>
      </div>
    );
  }

  const costSavingsValue = analytics.costSavings;
  // const costSavingsValue = analytics.totalBaselineCost - analytics.totalOptimizedCost;
  const costSavingsPercent = ((costSavingsValue / analytics.totalBaselineCost) * 100).toFixed(1);

  const hasTimeData = analytics.employeeTimeComparison && analytics.employeeTimeComparison.length > 0;

  const totalVehicles = analytics.totalVehiclesAvailable || 1;
  const used = analytics.totalVehiclesUsed || 0;
  const unassigned = totalVehicles - used;
  const pieData = [
    { name: 'Occupied', value: used, color: '#FBBF24', otherValue: unassigned },
    { name: 'Unassigned', value: unassigned, color: '#4B5563', otherValue: used }
  ];

  const fleetUtilization = totalVehicles ? ((used / totalVehicles) * 100).toFixed(1) : '0';
  const avgOccupancy = used ? (analytics.totalEmployees / used).toFixed(1) : '0';

  return (
    <div className="min-h-screen bg-[#0C0E14] text-gray-300 p-6 font-sans overflow-y-auto pb-24">

      <div className="grid grid-cols-1 md:grid-cols-4 gap-6 mb-8">
        <div className="bg-[#151821] border border-gray-800 rounded-xl p-6 flex items-center gap-4">
          <div className="w-12 h-12 bg-yellow-500/10 rounded-lg flex items-center justify-center">
            <Star className="text-yellow-400" size={24} />
          </div>
          <div>
            <p className="text-sm font-mono text-gray-500 uppercase tracking-wider">Total Score</p>
            <p className="text-3xl font-bold text-yellow-400">{analytics.totalScore}</p>
          </div>
        </div>

        <div className="bg-[#151821] border border-gray-800 rounded-xl p-6 flex items-center gap-4">
          <div className="w-12 h-12 bg-emerald-500/10 rounded-lg flex items-center justify-center">
            <DollarSign className="text-emerald-400" size={24} />
          </div>
          <div>
            <p className="text-sm font-mono text-gray-500 uppercase tracking-wider">Total Optimized Cost</p>
            <p className="text-3xl font-bold text-emerald-400">₹ {analytics.totalOptimizedCost}</p>
          </div>
        </div>

        <div className="bg-[#151821] border border-gray-800 rounded-xl p-6 flex items-center gap-4">
          <div className="w-12 h-12 bg-purple-500/10 rounded-lg flex items-center justify-center">
            <TrendingDown className="text-purple-400" size={24} />
          </div>
          <div>
            <p className="text-sm font-mono text-gray-500 uppercase tracking-wider">Cost Saved</p>
            <p className="text-3xl font-bold text-purple-400">
              ₹ {costSavingsValue} <span className="text-sm text-gray-500">({costSavingsPercent}%)</span>
            </p>
          </div>
        </div>

        <div className="bg-[#151821] border border-gray-800 rounded-xl p-6 flex items-center gap-4">
          <div className="w-12 h-12 bg-amber-500/10 rounded-lg flex items-center justify-center">
            <Clock className="text-amber-400" size={24} />
          </div>
          <div>
            <p className="text-sm font-mono text-gray-500 uppercase tracking-wider">Time Saved</p>
            <p className="text-3xl font-bold text-amber-400">{analytics.timeSavings} <span className="text-sm text-gray-500">min</span></p>
          </div>
        </div>
      </div>

      <div className="flex flex-wrap justify-end gap-4 mb-8">
        <div className="bg-[#151821] border border-gray-800 rounded-lg px-6 py-3 inline-flex items-center gap-3">
          <MapPin className="w-5 h-5 text-emerald-400" />
          <span className="text-sm font-mono text-gray-500">Stops Serviced</span>
          <span className="text-xl font-bold text-white">{analytics.totalEmployees}</span>
        </div>

        <div className="bg-[#151821] border border-gray-800 rounded-lg px-6 py-3 inline-flex items-center gap-3">
          <span className="text-sm font-mono text-gray-500">Total Distance Travelled</span>
          <span className="text-xl font-bold text-cyan-400">{analytics.totalDistance} km</span>
        </div>

        <div className="bg-[#151821] border border-gray-800 rounded-lg px-6 py-3 inline-flex items-center gap-3">
          <Timer className="w-5 h-5 text-cyan-400" />
          <span className="text-sm font-mono text-gray-500">Total Optimized Time</span>
          <span className="text-xl font-bold text-cyan-400">{analytics.totalOptimizedTime} min</span>
        </div>
      </div>

      <div className="space-y-6 mb-8">
        <div className="bg-[#151821] border border-gray-800 rounded-lg p-6">
          <h2 className="text-sm font-mono text-gray-400 mb-6 uppercase flex items-center gap-2">
            <TrendingDown size={16} className="text-cyan-400" /> Cost: Baseline vs Optimized (₹)
          </h2>
          <div className="h-64">
            {analytics.perVehicleCostData && analytics.perVehicleCostData.length > 0 ? (
              <ResponsiveContainer width="100%" height="100%">
                <BarChart data={analytics.perVehicleCostData} barSize={40}>
                  <CartesianGrid strokeDasharray="3 3" stroke="#2D3748" vertical={false} />
                  <XAxis dataKey="name" stroke="#718096" fontSize={12} tickLine={false} axisLine={false} />
                  <YAxis stroke="#718096" fontSize={12} tickLine={false} axisLine={false} />
                  <Tooltip
                    contentStyle={{ backgroundColor: '#1F2937', border: '1px solid #374151', borderRadius: '8px', color: '#F9FAFB' }}
                    itemStyle={{ color: '#F9FAFB' }}
                    labelStyle={{ color: '#9CA3AF', fontWeight: 'bold' }}
                    formatter={formatValue}
                    cursor={{ fill: '#2D3748', opacity: 0.4 }}
                  />
                  <Bar
                    dataKey="baseline"
                    fill="#F56565"
                    radius={[4, 4, 0, 0]}
                    name="Baseline Cost"
                  />
                  <Bar
                    dataKey="optimized"
                    fill="#00E5FF"
                    radius={[4, 4, 0, 0]}
                    name="Optimized Cost"
                  />
                </BarChart>
              </ResponsiveContainer>
            ) : (
              <div className="h-full flex items-center justify-center text-gray-500 font-mono text-sm">
                No cost comparison data available.
              </div>
            )}
          </div>
        </div>

        <div className="bg-[#151821] border border-gray-800 rounded-lg p-6">
          <h2 className="text-sm font-mono text-gray-400 mb-6 uppercase flex items-center gap-2">
            <Clock size={16} className="text-cyan-400" /> Travel Time: Baseline vs Optimized (min)
          </h2>
          <div className="h-64 overflow-x-auto">
            {hasTimeData ? (
              <div
                style={{
                  width: `max(100%, ${analytics.employeeTimeComparison.length * 60}px)`,
                  height: '100%'
                }}
              >
                <ResponsiveContainer width="100%" height="100%">
                  <BarChart data={analytics.employeeTimeComparison} barSize={20}>
                    <CartesianGrid strokeDasharray="3 3" stroke="#2D3748" vertical={false} />
                    <XAxis
                      dataKey="employee_id"
                      stroke="#718096"
                      fontSize={10}
                      tickLine={false}
                      axisLine={false}
                      interval={0}
                      angle={-45}
                      textAnchor="end"
                      height={60}
                    />
                    <YAxis stroke="#718096" fontSize={12} tickLine={false} axisLine={false} />
                    <Tooltip
                      contentStyle={{ backgroundColor: '#1F2937', border: '1px solid #374151', borderRadius: '8px', color: '#F9FAFB' }}
                      itemStyle={{ color: '#F9FAFB' }}
                      labelStyle={{ color: '#9CA3AF', fontWeight: 'bold' }}
                      formatter={formatValue}
                      cursor={{ fill: '#2D3748', opacity: 0.4 }}
                    />
                    <Bar
                      dataKey="baselineTime"
                      fill="#F56565"
                      radius={[4, 4, 0, 0]}
                      name="Baseline Time"
                    />
                    <Bar
                      dataKey="optimizedTime"
                      fill="#00E5FF"
                      radius={[4, 4, 0, 0]}
                      name="Optimized Time"
                    />
                  </BarChart>
                </ResponsiveContainer>
              </div>
            ) : (
              <div className="h-full flex flex-col items-center justify-center text-gray-500 font-mono text-sm">
                <Clock className="w-8 h-8 mb-2 opacity-50" />
                <p>No time comparison data available.</p>
              </div>
            )}
          </div>
          {hasTimeData && analytics.employeeTimeComparison.length > 5 && (
            <p className="text-xs text-gray-500 mt-2 text-center">* Scroll horizontally for more employees</p>
          )}
        </div>
      </div>

      <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-8">
        <div className="bg-[#151821] border border-gray-800 rounded-lg p-6">
          <h2 className="text-sm font-mono text-gray-400 mb-6 uppercase flex items-center gap-2">
            <ShieldCheck size={16} className="text-cyan-400" /> Constraint Compliance
          </h2>
          <div className="flex flex-col gap-5">
            {analytics.compliance.map((item, idx) => (
              <div key={idx}>
                <div className="flex justify-between items-center mb-1">
                  <span className="text-gray-300 text-xs font-bold">{item.label}</span>
                  <span className="text-cyan-400 text-xs font-mono">{item.percent.toFixed(1)}%</span>
                </div>
                <div className="text-lg font-bold text-white font-mono mb-2">
                  {item.current} <span className="text-gray-600 text-sm">/ {item.max}</span>
                </div>
                <div className="w-full h-1 bg-gray-800 rounded-full overflow-hidden">
                  <div
                    className={`h-full ${item.percent > 100 ? 'bg-yellow-500' : item.percent > 0 ? 'bg-cyan-400' : 'bg-red-500'}`}
                    style={{ width: `${Math.min(item.percent, 100)}%` }}
                  ></div>
                </div>
              </div>
            ))}
          </div>
        </div>

        <div className="bg-gradient-to-br from-[#151821] to-[#1A1F2E] border border-gray-800 rounded-lg p-6 shadow-lg">
          <h2 className="text-sm font-mono text-gray-400 mb-6 uppercase flex items-center gap-2">
            <Truck size={16} className="text-cyan-400" /> Fleet Status
          </h2>

          <div className="flex flex-col md:flex-row items-start gap-6">
            <div className="w-48 h-48 flex-shrink-0">
              <ResponsiveContainer width="100%" height="100%">
                <PieChart>
                  <Pie
                    data={pieData}
                    cx="50%"
                    cy="50%"
                    innerRadius={50}
                    outerRadius={70}
                    paddingAngle={2}
                    dataKey="value"
                    startAngle={90}
                    endAngle={-270}
                  >
                    {pieData.map((entry, index) => (
                      <Cell key={`cell-${index}`} fill={entry.color} stroke="none" />
                    ))}
                  </Pie>
                  <Tooltip content={<PieTooltip />} />
                </PieChart>
              </ResponsiveContainer>
            </div>

            <div className="flex flex-col gap-3 flex-shrink-0">
              <div className="bg-[#0C0E14]/80 backdrop-blur-sm border border-cyan-500/20 rounded-xl p-5 text-center min-w-[160px] shadow-lg shadow-cyan-500/5">
                <h3 className="text-gray-400 text-xs font-bold mb-1 uppercase tracking-wider">Fleet Utilization</h3>
                <p className="text-2xl font-bold text-cyan-400">{fleetUtilization}%</p>
              </div>
              <div className="bg-[#0C0E14]/80 backdrop-blur-sm border border-emerald-500/20 rounded-xl p-5 text-center min-w-[160px] shadow-lg shadow-emerald-500/5">
                <h3 className="text-gray-400 text-xs font-bold mb-1 uppercase tracking-wider">Avg Occupancy</h3>
                <p className="text-2xl font-bold text-white">{avgOccupancy} <span className="text-sm text-emerald-400">pax/veh</span></p>
              </div>
            </div>

            <div className="flex-1 min-w-[140px]">
              <h3 className="text-xs font-mono text-gray-500 mb-2 uppercase tracking-wider flex items-center gap-1">
                <Truck size={12} className="text-gray-400" /> Unassigned
              </h3>
              <div className="bg-[#0C0E14] rounded-lg border border-gray-800 p-2 max-h-32 overflow-y-auto custom-scrollbar">
                {analytics.unassignedVehicleIds && analytics.unassignedVehicleIds.length > 0 ? (
                  <div className="space-y-1">
                    {analytics.unassignedVehicleIds.map((vid) => (
                      <div key={vid} className="text-xs text-gray-400 font-mono bg-[#1A1F2E] px-2 py-1 rounded border border-gray-700 hover:border-cyan-500/30 transition-colors">
                        {vid}
                      </div>
                    ))}
                  </div>
                ) : (
                  <div className="h-full flex items-center justify-center text-gray-500 font-mono text-xs italic p-2">
                    ✦ None ✦
                  </div>
                )}
              </div>
            </div>
          </div>

          <div className="flex justify-center gap-6 mt-4 pt-3 border-t border-gray-800">
            <div className="flex items-center gap-2">
              <div className="w-3 h-3 rounded-full bg-[#FBBF24] shadow-sm shadow-amber-500"></div>
              <span className="text-xs text-gray-300">Occupied <span className="text-amber-400 font-mono">({used})</span></span>
            </div>
            <div className="flex items-center gap-2">
              <div className="w-3 h-3 rounded-full bg-gray-600 shadow-sm"></div>
              <span className="text-xs text-gray-300">Unassigned <span className="text-gray-400 font-mono">({unassigned})</span></span>
            </div>
          </div>
        </div>
      </div>

      <div className="bg-[#151821] border border-gray-800 rounded-lg p-6">
        <h2 className="text-sm font-mono text-gray-400 mb-4 uppercase flex items-center gap-2">
          <AlertTriangle size={16} className="text-amber-400" /> Constraint Violations
        </h2>
        {analytics.violations && analytics.violations.length > 0 ? (
          <div className="overflow-x-auto">
            <table className="w-full text-left border-collapse">
              <thead>
                <tr className="border-b border-gray-800">
                  <th className="pb-2 text-xs font-mono text-gray-500">Employee ID</th>
                  <th className="pb-2 text-xs font-mono text-gray-500">Violation Type</th>
                  <th className="pb-2 text-xs font-mono text-gray-500">Expected</th>
                  <th className="pb-2 text-xs font-mono text-gray-500">Actual</th>
                </tr>
              </thead>
              <tbody className="divide-y divide-gray-800">
                {analytics.violations.map((v, idx) => (
                  <tr key={idx} className="hover:bg-gray-800/20">
                    <td className="py-2 text-sm font-mono text-amber-400">{v.employee_id}</td>
                    <td className="py-2 text-sm text-gray-300">{v.type}</td>
                    <td className="py-2 text-sm text-gray-400">{v.expected}</td>
                    <td className="py-2 text-sm text-gray-400">{v.actual}</td>
                  </tr>
                ))}
              </tbody>
            </table>
          </div>
        ) : (
          <div className="flex items-center justify-center py-8 text-gray-500 font-mono text-sm">
            <ShieldCheck className="w-5 h-5 mr-2 text-emerald-500" />
            No constraint violations found
          </div>
        )}
      </div>
    </div>
  );
}