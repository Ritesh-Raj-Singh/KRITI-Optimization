"use client";

import React, { useState, useMemo } from 'react';
import {
  Heart, AlertTriangle, Clock, Car, Users, TrendingUp, TrendingDown,
  ChevronDown, ChevronUp, Star, Shield, Zap, Eye, Target, Activity
} from 'lucide-react';
import {
  BarChart, Bar, XAxis, YAxis, Tooltip, ResponsiveContainer,
  RadarChart, Radar, PolarGrid, PolarAngleAxis, PolarRadiusAxis,
  ScatterChart, Scatter, Cell, CartesianGrid, PieChart, Pie,
  Legend
} from 'recharts';

// ═══════════════════════════════════════════════
// ███  SATISFACTION SCORING ENGINE (Frontend)  ███
// ═══════════════════════════════════════════════

const WEIGHTS = { time: 0.45, vehicle: 0.30, sharing: 0.25 };

const sharingPrefToMax = (pref) => {
  if (!pref) return 1;
  const p = pref.toString().toLowerCase();
  if (p === 'single') return 1;
  if (p === 'double') return 2;
  if (p === 'triple') return 3;
  if (p === 'any') return 99;
  return 1;
};

const timeToMinutes = (timeStr) => {
  if (!timeStr) return 0;
  const parts = timeStr.split(':').map(Number);
  return parts[0] * 60 + (parts[1] || 0);
};

// Drop scoring — 3-tier system:
//   Tier 1: On-time or early (dropMin <= latestDrop)         → 5.0
//   Tier 2: Within extended priority window                  → linear decay 5 → 3
//   Tier 3: Beyond extended window                           → linear decay 3 → 1
const computeDropScore = (dropMin, latestDrop, latestDropWithDelay) => {
  // Early or on-time → full score
  if (dropMin <= latestDrop) {
    return { score: 5.0, zone: "OnTimeOrEarly" };
  }

  const extensionWindow = latestDropWithDelay - latestDrop; // priority delay minutes

  // Within the extended priority window → mild penalty (5 → 3)
  if (extensionWindow > 0 && dropMin <= latestDropWithDelay) {
    const ratio = (dropMin - latestDrop) / extensionWindow;
    const score = 5 - ratio * 2; // linearly 5 → 3
    return { score: Math.max(3, score), zone: "MildLate" };
  }

  // Beyond extended window → severe penalty (3 → 1)
  const beyondExtended = dropMin - latestDropWithDelay;
  const maxOvershoot = Math.max(extensionWindow, 15); // at least 15 min range
  const ratio = Math.min(1, beyondExtended / maxOvershoot);
  const score = 3 - ratio * 2; // linearly 3 → 1
  return { score: Math.max(1, score), zone: "SevereLate" };
};

// Sharing personal-space-erosion sigmoid
const computeSharingScore = (tripSize, maxAllowed) => {
  const excess = Math.max(0, tripSize - maxAllowed);
  if (excess === 0) return { score: 5.00, rating: 'Excellent', excess: 0 };
  if (excess === 1) return { score: 1.71, rating: 'Poor', excess };
  return { score: 1.00, rating: 'Critical', excess };
};

// Vehicle expectation-gradient
const computeVehicleScore = (preference, assigned) => {
  const pref = (preference || 'normal').toLowerCase();
  const asgn = (assigned || 'normal').toLowerCase();
  if (pref === 'any') return { score: 5.00, rating: 'Excellent' };
  if (pref === 'normal' && (asgn === 'normal' || asgn === 'premium')) return { score: 5.00, rating: 'Excellent' };
  if (pref === 'premium' && asgn === 'premium') return { score: 5.00, rating: 'Excellent' };
  if (pref === 'premium' && asgn === 'normal') return { score: 2.00, rating: 'Poor' };
  return { score: 3.00, rating: 'Fair' };
};

const getRating = (score) => {
  if (score >= 4.5) return 'Excellent';
  if (score >= 3.5) return 'Good';
  if (score >= 2.5) return 'Fair';
  if (score >= 1.5) return 'Poor';
  return 'Critical';
};

// ═══ Build full satisfaction data from mapData ═══
function computeSatisfaction(mapData) {
  const { rawAssignments, routes, satisfactionInputs } = mapData || {};
  if (!satisfactionInputs || !rawAssignments || rawAssignments.length === 0) return null;

  const { employeeDetails, vehicleDetailsMap, priorityDelays } = satisfactionInputs;

  // Build occupancy events per vehicle from assignments
  const vehicleTrips = {};
  rawAssignments.forEach(row => {
    const vId = row.vehicle_id;
    if (!vehicleTrips[vId]) vehicleTrips[vId] = [];
    vehicleTrips[vId].push({
      empId: row.employee_id,
      pickup: timeToMinutes(row.pickup_time),
      drop: timeToMinutes(row.drop_time),
    });
  });

  // For each employee, find max concurrent riders during their trip
  const getMaxOccupancy = (empId, vehicleId, pickupMin, dropMin) => {
    const trips = vehicleTrips[vehicleId] || [];
    // Count how many riders overlap with this employee's ride window
    let maxOcc = 0;
    // Sample at each event boundary
    const events = [];
    trips.forEach(t => {
      events.push({ time: t.pickup, delta: 1 });
      events.push({ time: t.drop, delta: -1 });
    });
    events.sort((a, b) => a.time - b.time || a.delta - b.delta);
    
    let occ = 0;
    let maxDuring = 0;
    for (const ev of events) {
      occ += ev.delta;
      if (ev.time >= pickupMin && ev.time <= dropMin) {
        maxDuring = Math.max(maxDuring, occ);
      }
    }
    // Also check occupancy AT pickup time
    let occAtPickup = 0;
    for (const t of trips) {
      if (t.pickup <= pickupMin && t.drop > pickupMin) occAtPickup++;
    }
    return Math.max(maxDuring, occAtPickup);
  };

  // Find vehicle type for each vehicle
  const getVehicleCategory = (vehicleId) => {
    // Check vehicleDetailsMap first
    if (vehicleDetailsMap && vehicleDetailsMap[vehicleId]) {
      return vehicleDetailsMap[vehicleId].type || 'normal';
    }
    // Fallback: get from assignment row
    const row = rawAssignments.find(r => r.vehicle_id === vehicleId);
    return (row?.category || 'normal').toLowerCase();
  };

  const details = [];
  let totalTimeScore = 0, totalVehicleScore = 0, totalSharingScore = 0, totalOverall = 0;
  let dropScoreSum = 0;
  let windowExceeded = 0, vehicleViolations = 0, sharingViolations = 0;
  let sharingCompliant = 0, vehicleCompliant = 0;

  rawAssignments.forEach(row => {
    const empId = row.employee_id;
    const empIdLower = empId.toLowerCase();
    const stripped = empIdLower.replace(/^[a-z0]+/i, '');
    const emp = employeeDetails[empId] || employeeDetails[empIdLower] || employeeDetails[stripped];
    if (!emp) return;

    const pickupMin = timeToMinutes(row.pickup_time);
    const dropMin = timeToMinutes(row.drop_time);
    const priority = emp.priority || 3;
    const delay = (priorityDelays && priorityDelays[priority]) || 0;
    const latestDropWithDelay = emp.latest_drop + delay;

    // Drop score — 3-tier: on-time/early → 5, mild late in extension → 5→3, beyond → 3→1
    const dropResult = computeDropScore(dropMin, emp.latest_drop, latestDropWithDelay);
    const dropScore = dropResult.score;
    const dropZone = dropResult.zone;
    const dropDelta = dropMin - emp.latest_drop;
    const withinWindow = dropMin <= latestDropWithDelay;

    // Time score = drop score only (no pickup component)
    const timeScore = dropScore;

    if (!withinWindow) windowExceeded++;

    // Vehicle score
    const vehicleCategory = getVehicleCategory(row.vehicle_id);
    const vehicleResult = computeVehicleScore(emp.vehicle_preference, vehicleCategory);
    if (vehicleResult.score >= 4.5) vehicleCompliant++;
    else vehicleViolations++;

    // Sharing score
    const maxAllowed = sharingPrefToMax(emp.sharing_preference);
    const tripSize = getMaxOccupancy(empId, row.vehicle_id, pickupMin, dropMin);
    const sharingResult = computeSharingScore(tripSize, maxAllowed);
    if (sharingResult.excess === 0) sharingCompliant++;
    else sharingViolations++;

    // Overall weighted
    const overall = timeScore * WEIGHTS.time + vehicleResult.score * WEIGHTS.vehicle + sharingResult.score * WEIGHTS.sharing;

    totalTimeScore += timeScore;
    totalVehicleScore += vehicleResult.score;
    totalSharingScore += sharingResult.score;
    totalOverall += overall;
    dropScoreSum += dropScore;

    details.push({
      employee_id: empId,
      priority,
      assigned: 'yes',
      vehicle_preference: emp.vehicle_preference || 'normal',
      vehicle_assigned: vehicleCategory,
      vehicle_score: vehicleResult.score,
      vehicle_rating: vehicleResult.rating,
      sharing_preference: emp.sharing_preference || 'single',
      trip_size: tripSize,
      sharing_max_allowed: maxAllowed,
      sharing_excess: sharingResult.excess,
      sharing_score: sharingResult.score,
      sharing_rating: sharingResult.rating,
      drop_delta_mins: dropDelta,
      drop_zone: dropZone,
      within_priority_window: withinWindow ? 'yes' : 'no',
      drop_score: dropScore,
      time_score: timeScore,
      time_rating: getRating(timeScore),
      overall_score: overall,
      overall_rating: getRating(overall),
    });
  });

  const n = details.length || 1;

  const summary = {
    version: 'v3-adaptive (live-computed)',
    scale: '1-5 (1=worst, 5=best)',
    weights: WEIGHTS,
    headcount: { total: details.length, assigned: details.length, unassigned: 0 },
    overall_score: totalOverall / n,
    time: {
      score: totalTimeScore / n,
      drop_score: dropScoreSum / n,
      window_exceeded_count: windowExceeded,
    },
    vehicle: {
      score: totalVehicleScore / n,
      compliance_rate_pct: parseFloat(((vehicleCompliant / n) * 100).toFixed(1)),
      violations: vehicleViolations,
    },
    sharing: {
      score: totalSharingScore / n,
      compliance_rate_pct: parseFloat(((sharingCompliant / n) * 100).toFixed(1)),
      violations: sharingViolations,
    },
  };

  return { summary, details };
}


// ═══════════════════════════════════════════
// ███  UI COMPONENTS  ███
// ═══════════════════════════════════════════

const PALETTE = {
  excellent: '#22d3ee', good: '#34d399', fair: '#fbbf24',
  poor: '#f97316', critical: '#ef4444', muted: '#64748b',
};
const RATING_COLORS = { Excellent: PALETTE.excellent, Good: PALETTE.good, Fair: PALETTE.fair, Poor: PALETTE.poor, Critical: PALETTE.critical };
const getRatingColor = (r) => RATING_COLORS[r] || PALETTE.muted;
const getScoreColor = (s) => {
  if (s >= 4.5) return PALETTE.excellent;
  if (s >= 3.5) return PALETTE.good;
  if (s >= 2.5) return PALETTE.fair;
  if (s >= 1.5) return PALETTE.poor;
  return PALETTE.critical;
};

const ScoreRing = ({ score, maxScore = 5, size = 120, label, color }) => {
  const radius = (size - 12) / 2;
  const circumference = 2 * Math.PI * radius;
  const fillColor = color || getScoreColor(score);
  const progress = (score / maxScore) * circumference;
  return (
    <div className="flex flex-col items-center gap-1">
      <div className="relative" style={{ width: size, height: size }}>
        <svg width={size} height={size} className="-rotate-90">
          <circle cx={size/2} cy={size/2} r={radius} fill="none" stroke="#1e293b" strokeWidth="6" />
          <circle cx={size/2} cy={size/2} r={radius} fill="none" stroke={fillColor} strokeWidth="6"
            strokeLinecap="round" strokeDasharray={circumference} strokeDashoffset={circumference - progress}
            style={{ transition: 'stroke-dashoffset 1.2s cubic-bezier(0.4, 0, 0.2, 1)' }} />
        </svg>
        <div className="absolute inset-0 flex flex-col items-center justify-center">
          <span className="text-2xl font-black font-mono" style={{ color: fillColor }}>{score.toFixed(2)}</span>
          <span className="text-[10px] text-slate-500 font-mono">/ {maxScore}</span>
        </div>
      </div>
      {label && <span className="text-xs font-bold text-slate-400 uppercase tracking-wider mt-1">{label}</span>}
    </div>
  );
};

const StatCard = ({ icon: Icon, label, value, sub, color = PALETTE.excellent }) => (
  <div className="relative bg-slate-900/60 backdrop-blur border border-slate-800 rounded-xl p-4 overflow-hidden hover:border-slate-700 transition-all">
    <div className="absolute top-0 right-0 w-20 h-20 rounded-bl-full opacity-5" style={{ background: color }} />
    <div className="flex items-start gap-3">
      <div className="w-9 h-9 rounded-lg flex items-center justify-center border" style={{ borderColor: color + '40', background: color + '10' }}>
        <Icon className="w-4 h-4" style={{ color }} />
      </div>
      <div className="flex-1 min-w-0">
        <p className="text-[11px] text-slate-500 font-mono uppercase tracking-wider">{label}</p>
        <p className="text-xl font-black font-mono mt-0.5" style={{ color }}>{value}</p>
        {sub && <p className="text-[10px] text-slate-500 mt-0.5">{sub}</p>}
      </div>
    </div>
  </div>
);

const CustomTooltip = ({ active, payload, label }) => {
  if (!active || !payload?.length) return null;
  return (
    <div className="bg-slate-900 border border-slate-700 rounded-lg px-3 py-2 shadow-xl text-xs font-mono">
      <p className="text-slate-400 mb-1">{label}</p>
      {payload.map((p, i) => (
        <p key={i} style={{ color: p.color || p.fill }}>{p.name}: <span className="font-bold">{typeof p.value === 'number' ? p.value.toFixed(2) : p.value}</span></p>
      ))}
    </div>
  );
};

const EmployeeRow = ({ emp }) => {
  const [expanded, setExpanded] = useState(false);
  const overallColor = getScoreColor(emp.overall_score);
  const ratingColor = getRatingColor(emp.overall_rating);
  return (
    <>
      <tr onClick={() => setExpanded(!expanded)} className="cursor-pointer hover:bg-slate-800/40 transition-colors border-b border-slate-800/50">
        <td className="p-3 font-mono text-sm font-bold text-cyan-300">{emp.employee_id}</td>
        <td className="p-3">
          <span className={`px-2 py-0.5 rounded text-[10px] font-bold uppercase tracking-wider border ${emp.priority <= 2 ? 'bg-amber-900/20 border-amber-500/30 text-amber-300' : 'bg-slate-800 border-slate-600 text-slate-400'}`}>P{emp.priority}</span>
        </td>
        <td className="p-3">
          <div className="flex items-center gap-2">
            <div className="w-16 h-1.5 bg-slate-800 rounded-full overflow-hidden">
              <div className="h-full rounded-full transition-all duration-700" style={{ width: `${(emp.overall_score / 5) * 100}%`, background: overallColor }} />
            </div>
            <span className="font-mono text-sm font-bold" style={{ color: overallColor }}>{emp.overall_score.toFixed(2)}</span>
          </div>
        </td>
        <td className="p-3"><span className="px-2 py-0.5 rounded-full text-[10px] font-bold border" style={{ color: ratingColor, borderColor: ratingColor + '40', background: ratingColor + '10' }}>{emp.overall_rating}</span></td>
        <td className="p-3 font-mono text-xs" style={{ color: getScoreColor(emp.time_score) }}>{emp.time_score.toFixed(2)}</td>
        <td className="p-3 font-mono text-xs" style={{ color: getScoreColor(emp.vehicle_score) }}>{emp.vehicle_score.toFixed(2)}</td>
        <td className="p-3 font-mono text-xs" style={{ color: getScoreColor(emp.sharing_score) }}>{emp.sharing_score.toFixed(2)}</td>
        <td className="p-3 text-right">{expanded ? <ChevronUp className="w-4 h-4 text-slate-500 inline" /> : <ChevronDown className="w-4 h-4 text-slate-500 inline" />}</td>
      </tr>
      {expanded && (
        <tr className="bg-slate-800/20">
          <td colSpan="8" className="p-0">
            <div className="p-4 grid grid-cols-3 gap-4 text-xs font-mono border-b border-slate-700/50">
              <div className="bg-slate-900/50 rounded-lg p-3 border border-slate-800">
                <p className="text-cyan-400 font-bold text-[11px] uppercase tracking-wider mb-2 flex items-center gap-1"><Clock className="w-3 h-3" /> Drop-off Breakdown</p>
                <div className="space-y-1.5 text-slate-300">
                  <div className="flex justify-between"><span className="text-slate-500">Drop Delta</span><span>{emp.drop_delta_mins > 0 ? '+' : ''}{emp.drop_delta_mins} min</span></div>
                  <div className="flex justify-between"><span className="text-slate-500">Drop Zone</span><span style={{ color: emp.drop_zone?.includes('Severe') ? PALETTE.critical : emp.drop_zone?.includes('Mild') ? PALETTE.fair : PALETTE.good }}>{emp.drop_zone}</span></div>
                  <div className="flex justify-between"><span className="text-slate-500">In Window?</span><span className={emp.within_priority_window === 'yes' ? 'text-emerald-400' : 'text-red-400'}>{emp.within_priority_window}</span></div>
                  <div className="flex justify-between border-t border-slate-700 pt-1.5 mt-1"><span className="text-slate-400 font-bold">Drop Score</span><span className="font-bold" style={{ color: getScoreColor(emp.drop_score) }}>{emp.drop_score.toFixed(2)}</span></div>
                </div>
              </div>
              <div className="bg-slate-900/50 rounded-lg p-3 border border-slate-800">
                <p className="text-purple-400 font-bold text-[11px] uppercase tracking-wider mb-2 flex items-center gap-1"><Car className="w-3 h-3" /> Vehicle Match</p>
                <div className="space-y-1.5 text-slate-300">
                  <div className="flex justify-between"><span className="text-slate-500">Preference</span><span className="capitalize">{emp.vehicle_preference}</span></div>
                  <div className="flex justify-between"><span className="text-slate-500">Assigned</span><span className="capitalize text-purple-300">{emp.vehicle_assigned}</span></div>
                  <div className="flex justify-between"><span className="text-slate-500">Rating</span><span style={{ color: getRatingColor(emp.vehicle_rating) }}>{emp.vehicle_rating}</span></div>
                  <div className="flex justify-between border-t border-slate-700 pt-1.5 mt-1"><span className="text-slate-400 font-bold">Score</span><span className="font-bold" style={{ color: getScoreColor(emp.vehicle_score) }}>{emp.vehicle_score.toFixed(2)}</span></div>
                </div>
              </div>
              <div className="bg-slate-900/50 rounded-lg p-3 border border-slate-800">
                <p className="text-amber-400 font-bold text-[11px] uppercase tracking-wider mb-2 flex items-center gap-1"><Users className="w-3 h-3" /> Sharing Comfort</p>
                <div className="space-y-1.5 text-slate-300">
                  <div className="flex justify-between"><span className="text-slate-500">Preference</span><span className="capitalize">{emp.sharing_preference}</span></div>
                  <div className="flex justify-between"><span className="text-slate-500">Trip Size</span><span>{emp.trip_size} riders</span></div>
                  <div className="flex justify-between"><span className="text-slate-500">Max Allowed</span><span>{emp.sharing_max_allowed}</span></div>
                  <div className="flex justify-between"><span className="text-slate-500">Excess</span><span className={emp.sharing_excess > 0 ? 'text-red-400 font-bold' : 'text-emerald-400'}>{emp.sharing_excess}</span></div>
                  <div className="flex justify-between"><span className="text-slate-500">Rating</span><span style={{ color: getRatingColor(emp.sharing_rating) }}>{emp.sharing_rating}</span></div>
                  <div className="flex justify-between border-t border-slate-700 pt-1.5 mt-1"><span className="text-slate-400 font-bold">Score</span><span className="font-bold" style={{ color: getScoreColor(emp.sharing_score) }}>{emp.sharing_score.toFixed(2)}</span></div>
                </div>
              </div>
            </div>
          </td>
        </tr>
      )}
    </>
  );
};


// ═══════════════════════════════════════════
// ███  MAIN DASHBOARD COMPONENT  ███
// ═══════════════════════════════════════════

export default function SatisfactionDashboard({ data }) {
  const [sortKey, setSortKey] = useState('overall_score');
  const [sortDir, setSortDir] = useState('desc');
  const [filterRating, setFilterRating] = useState('all');

  // Compute everything from mapData
  const result = useMemo(() => computeSatisfaction(data), [data]);

  if (!result) {
    return (
      <div className="w-full h-full flex flex-col items-center justify-center bg-[#0C0E14] gap-4 p-8">
        <Heart className="w-14 h-14 text-slate-700" />
        <p className="text-slate-500 font-mono text-sm text-center max-w-md">
          Run an optimization first. Satisfaction scores will be computed automatically from your assignment results.
        </p>
      </div>
    );
  }

  const { summary: s, details } = result;

  // ─── Derived Chart Data ───
  const ratingDistribution = (() => {
    const counts = { Excellent: 0, Good: 0, Fair: 0, Poor: 0, Critical: 0 };
    details.forEach(d => { if (counts[d.overall_rating] !== undefined) counts[d.overall_rating]++; });
    return Object.entries(counts).map(([name, value]) => ({ name, value, color: RATING_COLORS[name] }));
  })();

  const dropZoneDistribution = (() => {
    const zones = {};
    details.forEach(d => {
      const z = d.drop_zone || 'Unknown';
      zones[z] = (zones[z] || 0) + 1;
    });
    const zoneColors = { 'OnTimeOrEarly': PALETTE.excellent, 'MildLate': PALETTE.fair, 'SevereLate': PALETTE.critical };
    return Object.entries(zones).map(([name, value]) => ({
      name: name === 'OnTimeOrEarly' ? 'On Time / Early' : name === 'MildLate' ? 'Mild Late (Priority Window)' : name === 'SevereLate' ? 'Severe Late (Beyond Window)' : name,
      value, color: zoneColors[name] || PALETTE.muted
    }));
  })();

  const scoreDistribution = (() => {
    const buckets = [
      { range: '1.0–1.9', min: 1, max: 2, count: 0, color: PALETTE.critical },
      { range: '2.0–2.9', min: 2, max: 3, count: 0, color: PALETTE.poor },
      { range: '3.0–3.4', min: 3, max: 3.5, count: 0, color: PALETTE.fair },
      { range: '3.5–3.9', min: 3.5, max: 4, count: 0, color: PALETTE.good },
      { range: '4.0–4.4', min: 4, max: 4.5, count: 0, color: '#2dd4bf' },
      { range: '4.5–5.0', min: 4.5, max: 5.01, count: 0, color: PALETTE.excellent },
    ];
    details.forEach(d => { const b = buckets.find(b => d.overall_score >= b.min && d.overall_score < b.max); if (b) b.count++; });
    return buckets;
  })();

  const radarData = [
    { dimension: 'Drop-off', score: s.time?.drop_score || 0, fullMark: 5 },
    { dimension: 'Vehicle', score: s.vehicle?.score || 0, fullMark: 5 },
    { dimension: 'Sharing', score: s.sharing?.score || 0, fullMark: 5 },
  ];

  const scatterData = details.map(d => ({
    x: d.sharing_score, y: d.time_score, id: d.employee_id, rating: d.overall_rating, overall: d.overall_score,
  }));

  // Sort & filter
  let filtered = filterRating === 'all' ? [...details] : details.filter(d => d.overall_rating === filterRating);
  const sorted = filtered.sort((a, b) => {
    const aVal = a[sortKey] ?? 0, bVal = b[sortKey] ?? 0;
    return sortDir === 'desc' ? bVal - aVal : aVal - bVal;
  });

  const handleSort = (key) => {
    if (sortKey === key) setSortDir(d => d === 'desc' ? 'asc' : 'desc');
    else { setSortKey(key); setSortDir('desc'); }
  };

  return (
    <div className="w-full h-full bg-[#0C0E14] text-white overflow-y-auto [&::-webkit-scrollbar]:hidden [-ms-overflow-style:'none'] [scrollbar-width:'none']">
      <div className="max-w-[1440px] mx-auto px-8 py-8">

        {/* ═══ HEADER ═══ */}
        <div className="mb-8">
          <div className="flex items-center gap-3 mb-1">
            <div className="w-10 h-10 rounded-xl bg-gradient-to-br from-rose-500 to-purple-600 flex items-center justify-center shadow-[0_0_20px_rgba(244,63,94,0.3)]">
              <Heart className="w-5 h-5 text-white" />
            </div>
            <div>
              <h1 className="text-2xl font-black tracking-tight">Employee Satisfaction Analysis</h1>
              <p className="text-slate-500 text-xs font-mono tracking-wider">
                LIVE COMPUTED &nbsp;·&nbsp; SCALE: 1–5 &nbsp;·&nbsp;
                WEIGHTS: T:{(WEIGHTS.time * 100)}% V:{(WEIGHTS.vehicle * 100)}% S:{(WEIGHTS.sharing * 100)}%
              </p>
            </div>
          </div>
        </div>

        {/* ═══ SCORE RINGS + STATS ═══ */}
        <div className="grid grid-cols-12 gap-5 mb-8">
          <div className="col-span-3 bg-slate-900/50 backdrop-blur border border-slate-800 rounded-xl p-6 flex flex-col items-center justify-center">
            <ScoreRing score={s.overall_score} size={140} label="Overall Score" />
            <div className="mt-3 px-3 py-1 rounded-full text-xs font-bold border" style={{
              color: getScoreColor(s.overall_score), borderColor: getScoreColor(s.overall_score) + '40', background: getScoreColor(s.overall_score) + '10'
            }}>{s.headcount.total} Employees Scored</div>
          </div>

          <div className="col-span-5 bg-slate-900/50 backdrop-blur border border-slate-800 rounded-xl p-5 flex items-center justify-around">
            <ScoreRing score={s.time.score} size={100} label="Time" color="#22d3ee" />
            <ScoreRing score={s.vehicle.score} size={100} label="Vehicle" color="#a855f7" />
            <ScoreRing score={s.sharing.score} size={100} label="Sharing" color="#fbbf24" />
          </div>

          <div className="col-span-4 grid grid-cols-2 gap-3">
            <StatCard icon={Clock} label="Drop Score" value={s.time.drop_score.toFixed(2)} sub={`${s.time.window_exceeded_count} exceeded window`} color={getScoreColor(s.time.drop_score)} />
            <StatCard icon={Target} label="Time Score" value={s.time.score.toFixed(2)} sub="Drop-off based" color={getScoreColor(s.time.score)} />
            <StatCard icon={Shield} label="Vehicle Match" value={`${s.vehicle.compliance_rate_pct}%`} sub={`${s.vehicle.violations} violations`} color={s.vehicle.compliance_rate_pct >= 90 ? PALETTE.excellent : PALETTE.fair} />
            <StatCard icon={Users} label="Sharing Comfort" value={`${s.sharing.compliance_rate_pct}%`} sub={`${s.sharing.violations} violations`} color={s.sharing.compliance_rate_pct >= 50 ? PALETTE.good : PALETTE.critical} />
          </div>
        </div>

        {/* ═══ CHARTS ROW 1 ═══ */}
        <div className="grid grid-cols-12 gap-5 mb-8">
          <div className="col-span-4 bg-slate-900/50 backdrop-blur border border-slate-800 rounded-xl p-5">
            <h3 className="text-sm font-bold text-slate-300 mb-3 flex items-center gap-2"><Activity className="w-4 h-4 text-cyan-400" /> Dimension Radar</h3>
            <ResponsiveContainer width="100%" height={220}>
              <RadarChart data={radarData}>
                <PolarGrid stroke="#1e293b" />
                <PolarAngleAxis dataKey="dimension" tick={{ fill: '#94a3b8', fontSize: 11, fontFamily: 'monospace' }} />
                <PolarRadiusAxis domain={[0, 5]} tick={false} axisLine={false} />
                <Radar dataKey="score" stroke="#22d3ee" fill="#22d3ee" fillOpacity={0.15} strokeWidth={2} dot={{ r: 4, fill: '#22d3ee' }} />
              </RadarChart>
            </ResponsiveContainer>
          </div>

          <div className="col-span-4 bg-slate-900/50 backdrop-blur border border-slate-800 rounded-xl p-5">
            <h3 className="text-sm font-bold text-slate-300 mb-3 flex items-center gap-2"><Star className="w-4 h-4 text-amber-400" /> Rating Distribution</h3>
            <ResponsiveContainer width="100%" height={220}>
              <PieChart>
                <Pie data={ratingDistribution.filter(r => r.value > 0)} dataKey="value" nameKey="name" cx="50%" cy="50%" innerRadius={45} outerRadius={75} paddingAngle={3} strokeWidth={0}>
                  {ratingDistribution.filter(r => r.value > 0).map((entry, i) => <Cell key={i} fill={entry.color} />)}
                </Pie>
                <Tooltip content={<CustomTooltip />} />
                <Legend iconType="circle" iconSize={8} formatter={(val) => <span className="text-xs text-slate-400 font-mono">{val}</span>} />
              </PieChart>
            </ResponsiveContainer>
          </div>

          <div className="col-span-4 bg-slate-900/50 backdrop-blur border border-slate-800 rounded-xl p-5">
            <h3 className="text-sm font-bold text-slate-300 mb-3 flex items-center gap-2"><Zap className="w-4 h-4 text-emerald-400" /> Score Distribution</h3>
            <ResponsiveContainer width="100%" height={220}>
              <BarChart data={scoreDistribution}>
                <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                <XAxis dataKey="range" tick={{ fill: '#64748b', fontSize: 10, fontFamily: 'monospace' }} />
                <YAxis tick={{ fill: '#64748b', fontSize: 10 }} />
                <Tooltip content={<CustomTooltip />} />
                <Bar dataKey="count" name="Employees" radius={[4, 4, 0, 0]}>
                  {scoreDistribution.map((entry, i) => <Cell key={i} fill={entry.color} />)}
                </Bar>
              </BarChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* ═══ CHARTS ROW 2 ═══ */}
        <div className="grid grid-cols-12 gap-5 mb-8">
          <div className="col-span-6 bg-slate-900/50 backdrop-blur border border-slate-800 rounded-xl p-5">
            <h3 className="text-sm font-bold text-slate-300 mb-3 flex items-center gap-2"><Eye className="w-4 h-4 text-purple-400" /> Sharing vs Time Score (per Employee)</h3>
            <ResponsiveContainer width="100%" height={260}>
              <ScatterChart>
                <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" />
                <XAxis type="number" dataKey="x" name="Sharing" domain={[0, 5.5]} tick={{ fill: '#64748b', fontSize: 10, fontFamily: 'monospace' }} label={{ value: 'Sharing Score', position: 'bottom', fill: '#64748b', fontSize: 10 }} />
                <YAxis type="number" dataKey="y" name="Time" domain={[0, 5.5]} tick={{ fill: '#64748b', fontSize: 10 }} label={{ value: 'Time Score', angle: -90, position: 'insideLeft', fill: '#64748b', fontSize: 10 }} />
                <Tooltip content={({ active, payload }) => {
                  if (!active || !payload?.length) return null;
                  const d = payload[0].payload;
                  return (
                    <div className="bg-slate-900 border border-slate-700 rounded-lg px-3 py-2 shadow-xl text-xs font-mono">
                      <p className="text-cyan-300 font-bold">{d.id}</p>
                      <p className="text-slate-400">Sharing: {d.x.toFixed(2)} · Time: {d.y.toFixed(2)}</p>
                      <p style={{ color: getRatingColor(d.rating) }}>{d.rating} ({d.overall.toFixed(2)})</p>
                    </div>
                  );
                }} />
                <Scatter data={scatterData}>
                  {scatterData.map((entry, i) => <Cell key={i} fill={getRatingColor(entry.rating)} fillOpacity={0.7} />)}
                </Scatter>
              </ScatterChart>
            </ResponsiveContainer>
          </div>

          <div className="col-span-6 bg-slate-900/50 backdrop-blur border border-slate-800 rounded-xl p-5">
            <h3 className="text-sm font-bold text-slate-300 mb-3 flex items-center gap-2"><TrendingDown className="w-4 h-4 text-rose-400" /> Drop-off Zone Distribution</h3>
            <ResponsiveContainer width="100%" height={260}>
              <BarChart data={dropZoneDistribution} layout="vertical">
                <CartesianGrid strokeDasharray="3 3" stroke="#1e293b" horizontal={false} />
                <XAxis type="number" tick={{ fill: '#64748b', fontSize: 10 }} />
                <YAxis dataKey="name" type="category" tick={{ fill: '#94a3b8', fontSize: 10, fontFamily: 'monospace' }} width={180} />
                <Tooltip content={<CustomTooltip />} />
                <Bar dataKey="value" name="Employees" radius={[0, 4, 4, 0]}>
                  {dropZoneDistribution.map((entry, i) => <Cell key={i} fill={entry.color} />)}
                </Bar>
              </BarChart>
            </ResponsiveContainer>
          </div>
        </div>

        {/* ═══ INSIGHTS ═══ */}
        <div className="mb-8 grid grid-cols-3 gap-4">
          <div className="bg-gradient-to-r from-emerald-950/40 to-transparent border border-emerald-900/30 rounded-xl p-4">
            <div className="flex items-center gap-2 mb-2"><TrendingUp className="w-4 h-4 text-emerald-400" /><span className="text-xs font-bold text-emerald-400 uppercase tracking-wider">Strength</span></div>
            <p className="text-sm text-slate-300">Vehicle compliance at <span className="font-bold text-emerald-300">{s.vehicle.compliance_rate_pct}%</span> with {s.vehicle.violations === 0 ? 'zero violations' : `${s.vehicle.violations} violations`}. {s.vehicle.compliance_rate_pct >= 90 ? 'Excellent vehicle matching.' : 'Room for vehicle assignment improvement.'}</p>
          </div>
          <div className="bg-gradient-to-r from-amber-950/40 to-transparent border border-amber-900/30 rounded-xl p-4">
            <div className="flex items-center gap-2 mb-2"><AlertTriangle className="w-4 h-4 text-amber-400" /><span className="text-xs font-bold text-amber-400 uppercase tracking-wider">Watch</span></div>
            <p className="text-sm text-slate-300"><span className="font-bold text-amber-300">{s.time.window_exceeded_count}</span> employees exceeded their drop-off time window. Average drop score is {s.time.drop_score.toFixed(2)}/5.</p>
          </div>
          <div className="bg-gradient-to-r from-red-950/40 to-transparent border border-red-900/30 rounded-xl p-4">
            <div className="flex items-center gap-2 mb-2"><AlertTriangle className="w-4 h-4 text-red-400" /><span className="text-xs font-bold text-red-400 uppercase tracking-wider">{s.sharing.compliance_rate_pct < 50 ? 'Critical' : 'Info'}</span></div>
            <p className="text-sm text-slate-300">Sharing compliance is <span className="font-bold text-red-300">{s.sharing.compliance_rate_pct}%</span> — {s.sharing.violations} employees have excess co-riders beyond their comfort preference.</p>
          </div>
        </div>

        {/* ═══ EMPLOYEE TABLE ═══ */}
        <div className="mb-6 flex items-center justify-between">
          <div>
            <h2 className="text-xl font-bold flex items-center gap-2"><Users className="text-rose-400" /> Employee-Level Breakdown</h2>
            <p className="text-slate-500 text-xs font-mono mt-1">Click any row to expand detailed dimension scores</p>
          </div>
          <div className="flex items-center gap-3">
            <select value={filterRating} onChange={e => setFilterRating(e.target.value)}
              className="bg-slate-900 border border-slate-700 text-sm font-mono rounded-lg px-3 py-1.5 text-slate-300 outline-none focus:border-cyan-500 cursor-pointer">
              <option value="all">All Ratings</option>
              <option value="Excellent">Excellent</option>
              <option value="Good">Good</option>
              <option value="Fair">Fair</option>
              <option value="Poor">Poor</option>
              <option value="Critical">Critical</option>
            </select>
            <span className="text-xs text-slate-500 font-mono">{sorted.length} employees</span>
          </div>
        </div>

        <div className="w-full overflow-hidden rounded-xl border border-slate-800 bg-slate-900/50 backdrop-blur-sm shadow-lg mb-24">
          <table className="w-full text-left border-collapse">
            <thead>
              <tr className="bg-slate-800/50 border-b border-slate-700">
                {[
                  { key: 'employee_id', label: 'Employee' },
                  { key: 'priority', label: 'Priority' },
                  { key: 'overall_score', label: 'Overall Score' },
                  { key: 'overall_rating', label: 'Rating' },
                  { key: 'time_score', label: 'Time' },
                  { key: 'vehicle_score', label: 'Vehicle' },
                  { key: 'sharing_score', label: 'Sharing' },
                ].map(col => (
                  <th key={col.key} onClick={() => handleSort(col.key)}
                    className="p-3 font-bold text-rose-400 text-xs uppercase tracking-wider cursor-pointer hover:text-rose-300 transition-colors select-none">
                    <span className="flex items-center gap-1">
                      {col.label}
                      {sortKey === col.key && (sortDir === 'desc' ? <ChevronDown className="w-3 h-3" /> : <ChevronUp className="w-3 h-3" />)}
                    </span>
                  </th>
                ))}
                <th className="p-3 w-8"></th>
              </tr>
            </thead>
            <tbody>
              {sorted.map((emp, idx) => <EmployeeRow key={emp.employee_id || idx} emp={emp} />)}
              {sorted.length === 0 && (
                <tr><td colSpan="8" className="p-12 text-center text-slate-500 font-mono italic">No matching employees found.</td></tr>
              )}
            </tbody>
          </table>
        </div>
      </div>
    </div>
  );
}
