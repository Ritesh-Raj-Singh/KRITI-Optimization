"use client";

import React, { useState, useRef, useEffect } from 'react';
import dynamic from 'next/dynamic';
import { motion } from "framer-motion";
import * as XLSX from 'xlsx';
import {
  FileText, ArrowLeft, ArrowRight, Loader2, ShieldCheck,
  Map as MapIcon, Car, BarChart3, Bell, User, Navigation, LayoutDashboard,
  Clock, Fuel, Users, MapPin, Timer, Download,Heart
} from 'lucide-react';

const CyberpunkGlobe = dynamic(() => import('../components/CyberpunkGlobe'), {
  ssr: false,
  loading: () => <div className="absolute inset-0 bg-slate-950" />
});

const MapBoard = dynamic(() => import('../components/MapBoard'), {
  ssr: false,
  loading: () => <div className="w-full h-full bg-slate-900 flex items-center justify-center text-cyan-500 font-mono animate-pulse">INITIALIZING SATELLITE LINK...</div>
});

import ResultsPanel from '../components/ResultsPanel';
import ControlPanel from '../components/ControlPanel';
import VeloraAnalytics from '../components/VeloraAnalytics';
import SatisfactionDashboard from '../components/SatisfactionDashboard';
const calculateDuration = (start, end) => {
  if (!start || !end || start === '--:--' || end === '--:--') return 'N/A';
  const [startH, startM] = start.split(':').map(Number);
  const [endH, endM] = end.split(':').map(Number);
  const startMins = startH * 60 + startM;
  const endMins = endH * 60 + endM;
  let diff = endMins - startMins;
  if (diff < 0) diff += 24 * 60;
  const hours = Math.floor(diff / 60);
  const mins = diff % 60;
  if (hours > 0) return `${hours} hr ${mins} min`;
  return `${mins} min`;

};

export default function HomePage() {
  const [result, setResult] = useState(null);
  const [view, setView] = useState('landing');
  const [activeTab, setActiveTab] = useState('map');
  const [file, setFile] = useState(null);
  const [optimizationLevel, setOptimizationLevel] = useState('optimal');
  const [isProcessing, setIsProcessing] = useState(false);
  const [mapData, setMapData] = useState({ pickups: [], dropoffs: [], routes: [], rawAssignments: [] });
  const [inputVehicles, setInputVehicles] = useState([]);
  const [selectedVehicleIndex, setSelectedVehicleIndex] = useState(null);
  const [simulatingVehicleIndex, setSimulatingVehicleIndex] = useState(null);
  const [sidebarWidth, setSidebarWidth] = useState(400);
  const [isResizing, setIsResizing] = useState(false);
  const sidebarRef = useRef(null);
  const fileInputRef = useRef(null);

  const handleDownloadCSV = () => {
    if (!mapData.csvData) return;
    const blob = new Blob([mapData.csvData], { type: 'text/csv;charset=utf-8;' });
    const url = URL.createObjectURL(blob);
    const link = document.createElement('a');
    link.href = url;
    link.setAttribute('download', 'velora_optimized_routes.csv');
    document.body.appendChild(link);
    link.click();
    document.body.removeChild(link);
    if (result.results.mem.csv_employee) {
      const lines = result.results.mem.csv_employee.split("\n");

      // remove first line
      const employeeCsv = lines.slice(1).join("\n");

      const blob2 = new Blob([employeeCsv], { type: "text/csv;charset=utf-8;" });
      const url2 = URL.createObjectURL(blob2);

      const link2 = document.createElement("a");
      link2.href = url2;
      link2.setAttribute("download", "employee_routes.csv");
      document.body.appendChild(link2);
      link2.click();
      document.body.removeChild(link2);
    } else {
      console.error("nope")
    }
  };

  const handleFileChange = async (e) => {
    const selectedFile = e.target.files[0];
    if (selectedFile) {
      setFile(selectedFile);

      try {
        const data = await selectedFile.arrayBuffer();
        const workbook = XLSX.read(data);
        const vehicleSheetName = workbook.SheetNames.find(name => name.toLowerCase().includes('vehicle')) || workbook.SheetNames[0];
        const worksheet = workbook.Sheets[vehicleSheetName];
        const jsonData = XLSX.utils.sheet_to_json(worksheet);

        const extractedVehicles = jsonData.map(row => ({
          vehicleId: row.vehicle_id || row['Vehicle ID'] || row.VehicleID || row.id,
          vehicleType: row.category || row.Type || row.type || 'Normal',
          propulsion: row.propulsion || row.Propulsion || 'N/A'
        })).filter(v => v.vehicleId);

        setInputVehicles(extractedVehicles);
      } catch (error) {
        console.error("Error parsing Excel file:", error);
      }
    }
  };

  const handleProceed = () => {
    if (!file) return;
    setIsProcessing(true);
    setTimeout(() => {
      setIsProcessing(false);
      setView('dashboard');
    }, 1000);
  };

  const handleDataGenerated = (data) => {
    setMapData(data);
    setActiveTab('map');
  };

  const resetApp = () => {
    setFile(null);
    setMapData({ pickups: [], dropoffs: [], routes: [], rawAssignments: [] });
    setInputVehicles([]);
    setOptimizationLevel('optimal');
    setSelectedVehicleIndex(null);
    setSimulatingVehicleIndex(null);
    window.location.reload();
  };

  const handleSimulate = (index) => {
    setSelectedVehicleIndex(index);
    setSimulatingVehicleIndex(index);
  };

  const handleSimulationEnd = () => {
    setSimulatingVehicleIndex(null);
  };

  useEffect(() => {
    if (selectedVehicleIndex !== simulatingVehicleIndex) {
      setSimulatingVehicleIndex(null);
    }
  }, [selectedVehicleIndex, simulatingVehicleIndex]);

  const startResizing = (e) => setIsResizing(true);

  useEffect(() => {
    const resize = (e) => {
      if (isResizing) {
        const newWidth = Math.min(Math.max(e.clientX, 300), 800);
        setSidebarWidth(newWidth);
      }
    };
    const stopResizing = () => setIsResizing(false);
    if (isResizing) {
      window.addEventListener("mousemove", resize);
      window.addEventListener("mouseup", stopResizing);
    }
    return () => {
      window.removeEventListener("mousemove", resize);
      window.removeEventListener("mouseup", stopResizing);
    }
  }, [isResizing]);

  let completeFleet = [];
  if (inputVehicles.length > 0) {
    completeFleet = inputVehicles.map(vehicle => {
      const activeRoute = (mapData.routes || []).find(r => r.vehicleId === vehicle.vehicleId);
      if (activeRoute) return activeRoute;

      return {
        vehicleId: vehicle.vehicleId,
        vehicleType: vehicle.vehicleType,
        occupancy: 'N/A',
        distance: 'N/A',
        duration: 'N/A',
        propulsion: 'N/A',
        isUnassigned: true
      };
    });
  } else {
    completeFleet = [...(mapData.routes || [])];
  }

  const sortedRoutes = completeFleet.sort((a, b) =>
    (a.vehicleId || '').localeCompare(b.vehicleId || '', undefined, { numeric: true })
  );

  const sortedAssignments = [...(mapData.rawAssignments || [])].sort((a, b) => {
    return (a.employee_id || '').localeCompare(b.employee_id || '', undefined, { numeric: true });
  });

  if (view === 'dashboard') {
    return (
      <motion.div initial={{ opacity: 0 }} animate={{ opacity: 1 }} transition={{ duration: 0.5 }} className={`relative w-full h-screen bg-slate-950 flex flex-col font-sans overflow-hidden text-white ${isResizing ? 'cursor-col-resize select-none' : ''}`}>

        <nav className="flex items-center justify-between px-6 h-16 bg-slate-900 border-b border-cyan-900/30 z-50 shadow-lg flex-none">
          <div className="flex items-center gap-6">
            <button onClick={resetApp} className="group flex items-center gap-2 text-slate-400 hover:text-cyan-400 transition-colors text-xs font-bold uppercase tracking-wider border border-slate-700 rounded px-3 py-1.5 hover:border-cyan-500/50 hover:bg-cyan-950/30">
              <ArrowLeft className="w-3 h-3 group-hover:-translate-x-1 transition-transform" /> BACK
            </button>
            <div className="h-6 w-px bg-slate-700"></div>
            <div className="flex items-center gap-2">
              <div className="w-8 h-8 bg-linear-to-tr from-cyan-600 to-blue-600 rounded-lg flex items-center justify-center text-white font-bold shadow-[0_0_15px_rgba(34,211,238,0.3)]">V</div>
              <span className="text-xl font-black tracking-widest text-white">VELORA</span>
            </div>
          </div>

          <div className="hidden md:flex items-center gap-1 bg-slate-950/50 p-1 rounded-lg border border-slate-800">
            <button onClick={() => setActiveTab('map')} className={`flex items-center gap-2 px-4 py-1.5 rounded-md text-sm font-bold transition-all ${activeTab === 'map' ? 'bg-cyan-500/10 text-cyan-400 border border-cyan-500/20 shadow-sm' : 'text-slate-500 hover:text-slate-300'}`}>
              <MapIcon className="w-4 h-4" /> Map View
            </button>
            <button onClick={() => setActiveTab('dashboard_view')} className={`flex items-center gap-2 px-4 py-1.5 rounded-md text-sm font-bold transition-all ${activeTab === 'dashboard_view' ? 'bg-cyan-500/10 text-cyan-400 border border-cyan-500/20 shadow-sm' : 'text-slate-500 hover:text-slate-300'}`}>
              <LayoutDashboard className="w-4 h-4" /> Dashboard
            </button>
            <button onClick={() => setActiveTab('analytics')} className={`flex items-center gap-2 px-4 py-1.5 rounded-md text-sm font-bold transition-all ${activeTab === 'analytics' ? 'bg-cyan-500/10 text-cyan-400 border border-cyan-500/20 shadow-sm' : 'text-slate-500 hover:text-slate-300'}`}>
              <BarChart3 className="w-4 h-4" /> Analytics
            </button>
    <button onClick={() => setActiveTab('satisfaction')} className={`flex items-center gap-2 px-4 py-1.5 rounded-md text-sm font-bold transition-all ${activeTab === 'satisfaction' ? 'bg-cyan-500/10 text-cyan-400 border border-cyan-500/20 shadow-sm' : 'text-slate-500 hover:text-slate-300'}`}>
  <Heart className="w-4 h-4" /> Satisfaction
</button>
          </div>

          <div className="flex items-center gap-4">

            {mapData.csvData && (
              <button
                onClick={handleDownloadCSV}
                className="flex items-center gap-2 px-4 py-1.5 bg-emerald-500/10 border border-emerald-500/40 hover:bg-emerald-500 hover:text-white text-emerald-400 rounded-md text-xs font-bold transition-all shadow-[0_0_10px_rgba(16,185,129,0.1)] hover:shadow-[0_0_15px_rgba(16,185,129,0.4)] mr-2 tracking-wider"
              >
                <Download className="w-4 h-4" /> EXPORT CSV
              </button>
            )}
          </div>
        </nav>

        <div className="flex-1 flex overflow-hidden bg-slate-900 relative">

          <div className={`absolute inset-0 flex ${activeTab === 'map' ? 'z-10' : 'z-0 invisible'}`}>
            <div ref={sidebarRef} style={{ width: sidebarWidth }} className="flex-none bg-slate-900 flex flex-col border-r border-slate-800 z-20 shadow-2xl relative">
              <div className="flex-none bg-slate-900 z-30 border-b border-slate-800 shadow-lg">
                <ControlPanel
                  setResult={setResult}
                  file={file}
                  optimizationLevel={optimizationLevel}
                  onDataGenerated={handleDataGenerated}
                  onReupload={resetApp}
                />
              </div>
              <div className="flex-1 overflow-y-auto p-3 custom-scrollbar bg-slate-900/50">
                <ResultsPanel
                  data={mapData}
                  onVehicleSelect={setSelectedVehicleIndex}
                  selectedIndex={selectedVehicleIndex}
                  onSimulateClick={handleSimulate}
                />
              </div>
              <div onMouseDown={startResizing} className={`absolute top-0 right-0 w-1.5 h-full cursor-col-resize z-50 hover:bg-cyan-500 transition-colors ${isResizing ? 'bg-cyan-500' : 'bg-transparent'}`} />
            </div>
            <div className="flex-1 relative bg-slate-900 z-10">
              <MapBoard
                pickups={mapData.pickups}
                dropoffs={mapData.dropoffs}
                routes={mapData.routes}
                selectedRouteIndex={selectedVehicleIndex}
                simulatingVehicleIndex={simulatingVehicleIndex}
                onSimulationEnd={handleSimulationEnd}
              />
            </div>
          </div>

          <div className={`absolute inset-0 bg-slate-900 ${activeTab === 'dashboard_view' ? 'z-20 overflow-y-auto' : 'z-0 hidden'}`}>
            <div className="w-full h-full p-8 bg-slate-900 overflow-y-auto text-white [&::-webkit-scrollbar]:hidden [-ms-overflow-style:'none'] [scrollbar-width:'none']">
              <div className="mb-8">
                <h2 className="text-2xl font-bold text-white flex items-center gap-2">
                  <LayoutDashboard className="text-cyan-400" /> Live Fleet Manifest
                </h2>
                <p className="text-slate-400 text-sm mt-1 font-mono">Synchronized with: {file?.name || 'No file'}</p>
              </div>

              <div className="w-full overflow-hidden rounded-xl border border-slate-800 bg-slate-900/50 backdrop-blur-sm mb-12 shadow-lg">
                <table className="w-full text-left border-collapse">
                  <thead>
                    <tr className="bg-slate-800/50 border-b border-slate-700">
                      <th className="p-4 font-bold text-cyan-400 text-xs uppercase tracking-wider">Vehicle ID</th>
                      <th className="p-4 font-bold text-cyan-400 text-xs uppercase tracking-wider">Type</th>
                      <th className="p-4 font-bold text-cyan-400 text-xs uppercase tracking-wider">Employees count</th>
                      <th className="p-4 font-bold text-cyan-400 text-xs uppercase tracking-wider">Distance</th>
                      <th className="p-4 font-bold text-cyan-400 text-xs uppercase tracking-wider">Est. Duration</th>
                      <th className="p-4 font-bold text-cyan-400 text-xs uppercase tracking-wider">Propulsion</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-800">
                    {sortedRoutes.length > 0 ? (
                      sortedRoutes.map((route, idx) => (
                        <tr key={idx} className="hover:bg-slate-800/30 transition-colors group">
                          <td className="p-4 font-mono text-white text-sm">
                            {route.vehicleId}
                          </td>
                          <td className="p-4">
                            <span className={`px-2 py-1 rounded text-[10px] border uppercase font-bold tracking-wider ${(route.vehicleType || 'Normal').toLowerCase() === 'premium'
                              ? 'bg-purple-900/30 border-purple-500/30 text-purple-300'
                              : 'bg-slate-800 border-slate-600 text-slate-400'
                              }`}>
                              {route.vehicleType || 'Normal'}
                            </span>
                          </td>
                          <td className="p-4">
                            {route.isUnassigned ? (
                              <span className="text-slate-500 font-mono text-sm font-bold pl-2">N/A</span>
                            ) : (
                              <span className="px-2 py-1 bg-cyan-950/40 rounded text-[10px] border border-cyan-500/30 text-cyan-300">
                                {route.occupancy || route.passengers?.length || 0}
                              </span>
                            )}
                          </td>
                          <td className="p-4 text-sm text-slate-300">
                            <div className="flex items-center gap-2 font-mono">
                              <Navigation className="w-3 h-3 text-cyan-500" />
                              {route.distance || 'N/A'}
                            </div>
                          </td>
                          <td className="p-4">
                            <div className="flex items-center gap-2 text-xs font-mono text-slate-300">
                              <Clock className="w-3 h-3 text-slate-500" />
                              {route.duration || 'N/A'}
                            </div>
                          </td>
                          <td className="p-4">
                            <div className="flex items-center gap-2 text-xs font-mono capitalize text-slate-300">
                              <Fuel className="w-3 h-3 text-slate-500" />
                              {route.propulsion || 'Electric'}
                            </div>
                          </td>
                        </tr>
                      ))
                    ) : (
                      <tr>
                        <td colSpan="6" className="p-12 text-center text-slate-500 font-mono italic">
                          Waiting for optimization data from server...
                        </td>
                      </tr>
                    )}
                  </tbody>
                </table>
              </div>

              <div className="mb-6">
                <h2 className="text-xl font-bold text-white flex items-center gap-2">
                  <Users className="text-emerald-400" /> Employee Assignments
                </h2>
                <p className="text-slate-400 text-sm mt-1 font-mono">Individual pickup schedules & vehicle assignments</p>
              </div>

              <div className="w-full overflow-hidden rounded-xl border border-slate-800 bg-slate-900/50 backdrop-blur-sm shadow-lg mb-24">
                <table className="w-full text-left border-collapse">
                  <thead>
                    <tr className="bg-slate-800/50 border-b border-slate-700">
                      <th className="p-4 font-bold text-emerald-400 text-xs uppercase tracking-wider">Employee ID</th>
                      <th className="p-4 font-bold text-emerald-400 text-xs uppercase tracking-wider">Assigned Vehicle</th>
                      <th className="p-4 font-bold text-emerald-400 text-xs uppercase tracking-wider">Type</th>
                      <th className="p-4 font-bold text-emerald-400 text-xs uppercase tracking-wider">Start Time</th>
                      <th className="p-4 font-bold text-emerald-400 text-xs uppercase tracking-wider">End Time</th>
                      <th className="p-4 font-bold text-emerald-400 text-xs uppercase tracking-wider">Total Time</th>
                    </tr>
                  </thead>
                  <tbody className="divide-y divide-slate-800">
                    {sortedAssignments.length > 0 ? (
                      sortedAssignments.map((assignment, idx) => (
                        <tr key={`emp-${idx}`} className="hover:bg-slate-800/30 transition-colors">
                          <td className="p-4 font-mono text-emerald-300 text-sm font-bold">
                            {assignment.employee_id || 'N/A'}
                          </td>
                          <td className="p-4 font-mono text-white text-sm">
                            <div className="flex items-center gap-2">
                              <Car className="w-4 h-4 text-slate-500" />
                              {assignment.vehicle_id || 'N/A'}
                            </div>
                          </td>
                          <td className="p-4">
                            <span className={`px-2 py-1 rounded text-[10px] border uppercase font-bold tracking-wider ${(assignment.category || 'Normal').toLowerCase() === 'premium'
                              ? 'bg-purple-900/30 border-purple-500/30 text-purple-300'
                              : 'bg-slate-800 border-slate-600 text-slate-400'
                              }`}>
                              {assignment.category || 'Normal'}
                            </span>
                          </td>
                          <td className="p-4 text-sm text-slate-300">
                            <div className="flex items-center gap-2 font-mono">
                              <MapPin className="w-3 h-3 text-emerald-500" />
                              {assignment.pickup_time || '--:--'}
                            </div>
                          </td>
                          <td className="p-4 text-sm text-slate-300">
                            <div className="flex items-center gap-2 font-mono">
                              <MapPin className="w-3 h-3 text-red-500" />
                              {assignment.drop_time || '--:--'}
                            </div>
                          </td>
                          <td className="p-4 text-sm text-slate-300">
                            <div className="flex items-center gap-2 font-mono">
                              <Timer className="w-3 h-3 text-cyan-500" />
                              {calculateDuration(assignment.pickup_time, assignment.drop_time)}
                            </div>
                          </td>
                        </tr>
                      ))
                    ) : (
                      <tr>
                        <td colSpan="6" className="p-12 text-center text-slate-500 font-mono italic">
                          Waiting for individual assignment data...
                        </td>
                      </tr>
                    )}
                  </tbody>
                </table>
              </div>
            </div>
          </div>

          <div className={`absolute inset-0 bg-[#0C0E14] ${activeTab === 'analytics' ? 'z-20 overflow-y-auto' : 'z-0 hidden'}`}>
            <VeloraAnalytics data={mapData.analytics} />
          </div>
                      <div className={`absolute inset-0 bg-[#0C0E14] ${activeTab === 'satisfaction' ? 'z-20 overflow-y-auto' : 'z-0 hidden'}`}>
  <SatisfactionDashboard data={mapData} />
</div>

        </div>
      </motion.div>
    );
  }

  return (
    <motion.div exit={{ opacity: 0, scale: 0.95, filter: "blur(10px)" }} transition={{ duration: 0.8 }} className="relative w-full min-h-screen overflow-hidden flex flex-col items-center justify-center font-sans bg-slate-950">
      <CyberpunkGlobe />
      <main className="relative z-10 w-full max-w-4xl px-6 flex flex-col items-center text-center space-y-12">
        <div className="space-y-4 animate-in fade-in slide-in-from-bottom-8 duration-1000">
          <h1 className="text-8xl md:text-7xl font-black tracking-tighter text-white drop-shadow-[0_0_40px_rgba(34,211,238,0.5)]">VELORA</h1>
          <p className="text-cyan-300/80 text-xl font-mono tracking-widest uppercase"> Smart Fleet Assignment & Route Optimizer</p>
        </div>

        <div className="w-full max-w-sm animate-in zoom-in duration-500 delay-150">
          <div onClick={() => !file && fileInputRef.current?.click()} className={`relative group cursor-pointer min-h-[260px] backdrop-blur-md border-2 border-dashed rounded-2xl transition-all duration-300 flex flex-col items-center justify-center p-6 shadow-[0_0_50px_rgba(0,0,0,0.5)] ${file ? 'border-emerald-500/50 bg-emerald-950/40' : 'border-white/20 hover:border-cyan-400 bg-slate-900/60 hover:bg-slate-900/80'}`}>
            <input type="file" ref={fileInputRef} onChange={handleFileChange} className="hidden" accept=".csv,.xlsx,.xls,.json" />

            {!file ? (
              <div className="flex flex-col items-center gap-5 pointer-events-none">
                <div className="w-14 h-14 rounded-xl bg-slate-800/50 border border-white/10 flex items-center justify-center shadow-lg group-hover:scale-110 transition-transform duration-300 group-hover:shadow-cyan-500/20">
                  <FileText className="w-7 h-7 text-cyan-400" />
                </div>
                <div className="space-y-1 text-center">
                  <h3 className="text-lg font-bold text-white tracking-tight">Upload Mission Data</h3>
                  <p className="text-xs text-cyan-200/50 font-mono">.XSLX</p>
                </div>
              </div>
            ) : (
              <div className="flex flex-col items-center w-full animate-in zoom-in">
                <ShieldCheck className="w-12 h-12 text-emerald-400 drop-shadow-[0_0_15px_rgba(52,211,153,0.5)] mb-4" />
                <div className="text-center w-full px-1">

                  <h3 className="text-lg font-bold text-white tracking-tight mb-5 truncate">{file.name}</h3>

                  <button onClick={(e) => { e.stopPropagation(); handleProceed(); }} className="w-full py-3 bg-gradient-to-r from-cyan-600 to-blue-600 hover:from-cyan-500 hover:to-blue-500 text-white text-sm font-bold rounded-xl shadow-[0_0_30px_rgba(34,211,238,0.4)] flex items-center justify-center gap-2 mb-5 transition-all active:scale-95">
                    {isProcessing ? <Loader2 className="w-4 h-4 animate-spin" /> : "INITIALIZE MAP"} <ArrowRight className="w-4 h-4" />
                  </button>

                  <div
                    className="w-full text-left"
                    onClick={(e) => e.stopPropagation()}
                  >
                    <select
                      value={optimizationLevel}
                      onChange={(e) => setOptimizationLevel(e.target.value)}
                      className="w-full bg-slate-950/80 border border-slate-700 text-white text-sm font-mono rounded-xl px-4 py-2.5 outline-none focus:border-cyan-400 focus:ring-1 focus:ring-cyan-400/50 transition-all cursor-pointer appearance-none"
                      style={{ backgroundImage: `url("data:image/svg+xml,%3Csvg xmlns='http://www.w3.org/2000/svg' fill='none' viewBox='0 0 24 24' stroke='%2322d3ee'%3E%3Cpath stroke-linecap='round' stroke-linejoin='round' stroke-width='2' d='M19 9l-7 7-7-7'%3E%3C/path%3E%3C/svg%3E")`, backgroundPosition: `right 12px center`, backgroundRepeat: `no-repeat`, backgroundSize: `16px` }}
                    >
                      <option value="ultra_fast">Ultra Fast - [15s] </option>
                      <option value="fast">Fast - [25s] </option>
                      <option value="optimal">Optimal - [60s] </option>
                    </select>
                  </div>

                </div>
              </div>
            )}
          </div>
        </div>
      </main>
    </motion.div>
  );
}
