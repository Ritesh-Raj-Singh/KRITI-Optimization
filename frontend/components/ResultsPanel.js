"use client";
import React from 'react';
import { Car, Clock, Navigation, User, AlertCircle, ChevronRight, Layers, Users, Play } from 'lucide-react';

export default function ResultsPanel({ data, onVehicleSelect, selectedIndex, onSimulateClick }) {
  if (!data || !data.routes || data.routes.length === 0) {
    return (
      <div className="flex flex-col items-center justify-center h-40 text-slate-500 space-y-2 pt-10">
        <AlertCircle className="w-6 h-6 opacity-50" />
        <span className="text-sm font-mono">NO_ACTIVE_VEHICLES</span>
      </div>
    );
  }

  return (
    <div className="space-y-2"> 
      
      <button 
        onClick={() => onVehicleSelect(null)}
        className={`
           w-full flex items-center justify-center gap-2 py-2 rounded-md text-[10px] font-bold uppercase tracking-wider transition-all border
           ${selectedIndex === null 
             ? 'bg-cyan-500/20 text-cyan-400 border-cyan-500/50' 
             : 'bg-slate-800 text-slate-400 border-slate-700 hover:bg-slate-700'}
        `}
      >
        <Layers className="w-3 h-3" />
        All Vehicle Routes
      </button>

      <div className="text-slate-500 text-[10px] font-bold uppercase tracking-widest px-1 mt-4 mb-2">
        Optimized Fleet ({data.routes.length})
      </div>

      {data.routes.map((route, index) => {
        const isSelected = selectedIndex === index;

        return (
          <div 
            key={index}
            onClick={() => onVehicleSelect(index)} 
            className={`
              cursor-pointer rounded-lg p-3 transition-all group relative border
              ${isSelected 
                ? 'bg-slate-800 border-cyan-500 shadow-[0_0_15px_rgba(34,211,238,0.1)]' 
                : 'bg-slate-800/40 border-slate-700/50 hover:bg-slate-800 hover:border-slate-600'
              }
            `}
          >
            <div className="flex items-center justify-between mb-3">
              <div className="flex items-center gap-3">
                <div className={`p-1.5 rounded transition-colors ${isSelected ? 'bg-cyan-500/20 text-cyan-400' : 'bg-slate-700/50 text-slate-400'}`}>
                  <Car className="w-4 h-4" />
                </div>
                <div>
                    <div className={`font-bold text-sm ${isSelected ? 'text-cyan-400' : 'text-slate-200'}`}>
                    {route.vehicleId || `Vehicle ${index + 1}`}
                    </div>
                    <div className="text-[10px] text-slate-500 uppercase">Active Route</div>
                </div>
              </div>
              <div className="flex items-center gap-2">
                <button
                  onClick={(e) => {
                    e.stopPropagation();
                    onSimulateClick(index);
                  }}
                  className="p-1.5 bg-slate-700 hover:bg-cyan-600 rounded-md transition-colors"
                  title="Simulate route"
                >
                  <Play className="w-3.5 h-3.5 text-white" />
                </button>
                {isSelected && <ChevronRight className="w-4 h-4 text-cyan-500 animate-pulse" />}
              </div>
            </div>

            <div className="grid grid-cols-2 gap-2 mb-3">
               <div className="bg-slate-900/60 px-2 py-1.5 rounded flex items-center gap-2 border border-white/5">
                 <Navigation className="w-3 h-3 text-emerald-500" />
                 <div>
                   <div className="text-[9px] text-slate-500 uppercase leading-none mb-0.5">Total Dist</div>
                   <div className="text-xs font-mono text-slate-200">
                      {route.distance || '0 km'}
                   </div>
                 </div>
               </div>
               <div className="bg-slate-900/60 px-2 py-1.5 rounded flex items-center gap-2 border border-white/5">
                 <Clock className="w-3 h-3 text-orange-400" />
                 <div>
                   <div className="text-[9px] text-slate-500 uppercase leading-none mb-0.5">Total Time</div>
                   <div className="text-xs font-mono text-slate-200">
                      {route.duration || '0 min'}
                   </div>
                 </div>
               </div>
            </div>

            <div className="border-t border-slate-700/30 pt-2 mt-2">
                <div className="flex items-center gap-1.5 text-[10px] text-slate-400 mb-1">
                    <Users className="w-3 h-3" />
                    <span className="uppercase font-bold tracking-wide">Passengers ({route.passengers?.length || 0})</span>
                </div>
                <div className="flex flex-wrap gap-1">
                    {route.passengers && route.passengers.length > 0 ? (
                        route.passengers.map((pid, idx) => (
                            <span key={idx} className="text-[10px] bg-slate-700/50 px-1.5 py-0.5 rounded text-slate-300 border border-slate-600/50">
                                {pid}
                            </span>
                        ))
                    ) : (
                        <span className="text-[10px] text-slate-600 italic">No passengers assigned</span>
                    )}
                </div>
            </div>

          </div>
        );
      })}
    </div>
  );
}