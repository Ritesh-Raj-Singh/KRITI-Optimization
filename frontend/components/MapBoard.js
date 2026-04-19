"use client";

import React, { useEffect, useState, useRef, useMemo } from 'react';
import { MapContainer, TileLayer, Marker, Popup, Polyline, useMap } from 'react-leaflet';
import L from 'leaflet';
import 'leaflet/dist/leaflet.css';

const createIcon = (url, size) => {
  if (typeof window === 'undefined') return null;
  return new L.Icon({
    iconUrl: url,
    iconSize: size,
    iconAnchor: [size[0] / 2, size[1]],
    popupAnchor: [0, -size[1] + 10],
  });
};

const pickupIcon = typeof window !== 'undefined' ? createIcon('https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-2x-green.png', [25, 41]) : null;
const dropIcon = typeof window !== 'undefined' ? createIcon('https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-2x-red.png', [25, 41]) : null;
const depotIcon = typeof window !== 'undefined' ? createIcon('https://raw.githubusercontent.com/pointhi/leaflet-color-markers/master/img/marker-icon-2x-gold.png', [25, 41]) : null;
const carIcon = typeof window !== 'undefined' ? createIcon('https://cdn-icons-png.flaticon.com/512/741/741407.png', [32, 32]) : null;

const haversineDistance = (lat1, lon1, lat2, lon2) => {
  const R = 6371;
  const dLat = (lat2 - lat1) * Math.PI / 180;
  const dLon = (lon2 - lon1) * Math.PI / 180;
  const a = Math.sin(dLat/2) * Math.sin(dLat/2) +
            Math.cos(lat1 * Math.PI / 180) * Math.cos(lat2 * Math.PI / 180) *
            Math.sin(dLon/2) * Math.sin(dLon/2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1-a));
  return R * c;
};

const computeCumulativeDistances = (path) => {
  if (!path || path.length === 0) return { cumDist: [0], total: 0 };
  const cumDist = [0];
  let total = 0;
  for (let i = 1; i < path.length; i++) {
    const [lat1, lng1] = path[i-1];
    const [lat2, lng2] = path[i];
    const dist = haversineDistance(lat1, lng1, lat2, lng2);
    total += dist;
    cumDist.push(total);
  }
  return { cumDist, total };
};

const closestPointOnSegment = (p, a, b) => {
  const [px, py] = p;
  const [ax, ay] = a;
  const [bx, by] = b;
  const ab = [bx - ax, by - ay];
  const ap = [px - ax, py - ay];
  const t = (ap[0] * ab[0] + ap[1] * ab[1]) / (ab[0] * ab[0] + ab[1] * ab[1] || 1);
  const clampedT = Math.max(0, Math.min(1, t));
  const closest = [ax + clampedT * ab[0], ay + clampedT * ab[1]];
  return [closest, clampedT];
};

const projectPointToPolyline = (point, polyline, cumDist) => {
  if (polyline.length < 2) return 0;
  let minDist = Infinity;
  let minIdx = 0;
  let minT = 0;
  for (let i = 0; i < polyline.length - 1; i++) {
    const p1 = polyline[i];
    const p2 = polyline[i + 1];
    const [closest, t] = closestPointOnSegment(point, p1, p2);
    const d = haversineDistance(point[0], point[1], closest[0], closest[1]);
    if (d < minDist) {
      minDist = d;
      minIdx = i;
      minT = t;
    }
  }
  const segStartDist = cumDist[minIdx];
  const segLen = cumDist[minIdx + 1] - cumDist[minIdx];
  return segStartDist + minT * segLen;
};

function AnimatedPolyline({ fullPositions, color, delay = 0 }) {
  const [visiblePositions, setVisiblePositions] = useState([]);
  
  useEffect(() => {
    if (!fullPositions || fullPositions.length === 0) return;
    setVisiblePositions([fullPositions[0]]);

    let animationInterval;
    const startTimer = setTimeout(() => {
      let currentIndex = 1;
      animationInterval = setInterval(() => {
        const batchSize = 8; 
        if (currentIndex < fullPositions.length) {
          const nextBatch = fullPositions.slice(0, currentIndex + batchSize);
          setVisiblePositions(nextBatch);
          currentIndex += batchSize;
        } else { 
          clearInterval(animationInterval); 
        }
      }, 16);
    }, delay);

    return () => {
      clearTimeout(startTimer);
      clearInterval(animationInterval);
    };
  }, [fullPositions, delay]);

  return <Polyline positions={visiblePositions} pathOptions={{ color: color, weight: 5, opacity: 0.9, lineCap: 'round' }} />;
}

function TraceRoute({ route, isSelected, isDimmed, simulateProgress }) {
  const routePath = route.path || [];
  if (!routePath || routePath.length === 0) return null;

  const { cumDist, total } = useMemo(() => computeCumulativeDistances(routePath), [routePath]);
  const color = route.color || '#3b82f6';
  const finalOpacity = isDimmed ? 0.2 : 1;
  const showAnimation = !isDimmed; 
  const animationDelay = isSelected ? 1500 : 0;

  let carMarker = null;

  if (simulateProgress !== undefined && simulateProgress > 0 && total > 0) {
    let carPosition;
    if (simulateProgress >= 1) {
      carPosition = [routePath[routePath.length - 1][0], routePath[routePath.length - 1][1]];
    } else {
      const targetDist = simulateProgress * total;
      let segmentIndex = 0;
      while (segmentIndex < cumDist.length - 1 && cumDist[segmentIndex + 1] < targetDist) {
        segmentIndex++;
      }
      const distStart = cumDist[segmentIndex];
      const distEnd = cumDist[segmentIndex + 1];
      const segFraction = (targetDist - distStart) / (distEnd - distStart || 1);
      const [lat1, lng1] = routePath[segmentIndex];
      const [lat2, lng2] = routePath[segmentIndex + 1];
      carPosition = [
        lat1 + (lat2 - lat1) * segFraction,
        lng1 + (lng2 - lng1) * segFraction
      ];
    }

    carMarker = (
      <Marker
        key={`${route.vehicleId}-sim-${simulateProgress.toFixed(4)}`}
        position={carPosition}
        icon={carIcon}
        opacity={finalOpacity}
      >
        <Popup>
          <div className="text-slate-900 font-sans">
            <h3 className="font-bold">{route.vehicleId}</h3>
            <div className="text-xs">
              Status: {simulateProgress >= 1 ? 'Trip Completed' : 'Simulating'}
            </div>
            {route.startTime && <div className="text-xs">Available from: {route.startTime}</div>}
          </div>
        </Popup>
      </Marker>
    );
  } else {
    carMarker = (
      <Marker
        key={`${route.vehicleId}-depot`}
        position={[routePath[0][0], routePath[0][1]]}
        icon={carIcon}
        opacity={finalOpacity}
      >
        <Popup>
          <div className="text-slate-900 font-sans">
            <h3 className="font-bold">{route.vehicleId}</h3>
            <div className="text-xs">Status: Idle (at depot)</div>
            {route.startTime && <div className="text-xs">Available from: {route.startTime}</div>}
          </div>
        </Popup>
      </Marker>
    );
  }

  return (
    <>
      {carMarker}
      {showAnimation ? (
        <AnimatedPolyline fullPositions={routePath} color={color} delay={animationDelay} />
      ) : (
        <Polyline positions={routePath} pathOptions={{ color: '#94a3b8', weight: 3, opacity: 0.2 }} />
      )}
    </>
  );
}

function MapController({ points, dropoffs, routes, selectedIndex }) {
  const map = useMap();

  useEffect(() => {
    if (!map) return;
    if (selectedIndex !== null && routes && routes[selectedIndex]) {
      const route = routes[selectedIndex];
      if (route.path && route.path.length > 0) {
        map.flyToBounds(L.latLngBounds(route.path), { 
           padding: [100, 100], 
           maxZoom: 15, 
           duration: 1.5, 
           animate: true 
        });
        return;
      }
    }
    if (selectedIndex === null) {
        const allCoords = [];
        (points || []).forEach(p => allCoords.push([p.lat, p.lng]));
        (dropoffs || []).forEach(d => allCoords.push([d.lat, d.lng]));
        (routes || []).forEach(r => {
            if(r.path) r.path.forEach(c => allCoords.push(c));
        });
        if (allCoords.length > 0) {
          map.flyToBounds(L.latLngBounds(allCoords), { 
            padding: [50, 50],
            duration: 1.0 
          });
        }
    }
  }, [selectedIndex, map, routes, points, dropoffs]);

  return null;
}

export default function MapBoard({ 
  pickups, 
  dropoffs, 
  routes, 
  selectedRouteIndex,
  simulatingVehicleIndex,
  onSimulationEnd
}) {
  const [isMounted, setIsMounted] = useState(false);
  const [simProgress, setSimProgress] = useState(0);
  const animationRef = useRef(null);
  const simStartTimeRef = useRef(null);
  
  const DURATION = 13000;

  useEffect(() => {
    setIsMounted(true);
  }, []);

  useEffect(() => {
    if (simulatingVehicleIndex === null) {
      if (animationRef.current) cancelAnimationFrame(animationRef.current);
      setSimProgress(0);
      simStartTimeRef.current = null;
      return;
    }

    const animate = (timestamp) => {
      if (!simStartTimeRef.current) simStartTimeRef.current = timestamp;
      const elapsed = timestamp - simStartTimeRef.current;
      let progress = Math.min(elapsed / DURATION, 1);
      setSimProgress(progress);

      if (progress < 1) {
        animationRef.current = requestAnimationFrame(animate);
      } else {
        if (onSimulationEnd) onSimulationEnd();
        simStartTimeRef.current = null;
      }
    };

    animationRef.current = requestAnimationFrame(animate);

    return () => {
      if (animationRef.current) cancelAnimationFrame(animationRef.current);
    };
  }, [simulatingVehicleIndex, DURATION, onSimulationEnd]);

  useEffect(() => {
    if (simulatingVehicleIndex !== null) {
      setSimProgress(0);
      simStartTimeRef.current = null;
    }
  }, [simulatingVehicleIndex]);

  useEffect(() => {
    return () => {
      if (animationRef.current) cancelAnimationFrame(animationRef.current);
    };
  }, []);

  const simulatingRoute = useMemo(() => {
    if (simulatingVehicleIndex === null || !routes || !routes[simulatingVehicleIndex]) return null;
    return routes[simulatingVehicleIndex];
  }, [simulatingVehicleIndex, routes]);

  const stopDistances = useMemo(() => {
    if (!simulatingRoute) return [];
    const route = simulatingRoute;
    const path = route.path || [];
    if (path.length === 0) return [];
    const { cumDist, total } = computeCumulativeDistances(path);
    const stops = route.stops || [];
    const validStops = stops.filter(s => s.lat != null && s.lng != null);
    const withDist = validStops.map(stop => {
      const point = [stop.lat, stop.lng];
      let distance = 0;
      if (path.length === 1) {
        distance = 0;
      } else {
        distance = projectPointToPolyline(point, path, cumDist);
      }
      return { ...stop, distance };
    });
    withDist.sort((a, b) => a.distance - b.distance);
    return withDist;
  }, [simulatingRoute]);

  const [displayStatus, setDisplayStatus] = useState({ title: '', message: '' });
  const lastUpdateRef = useRef(0);
  const THROTTLE_MS = 250;
  const HYSTERESIS = 0.02;

  useEffect(() => {
    if (!simulatingRoute || simProgress === undefined) {
      setDisplayStatus({ title: '', message: '' });
      return;
    }

    const now = Date.now();
    if (now - lastUpdateRef.current < THROTTLE_MS) return;
    lastUpdateRef.current = now;

    const route = simulatingRoute;
    const path = route.path || [];
    if (path.length === 0) return;

    const { total } = computeCumulativeDistances(path);
    const currentDist = simProgress * total;
    const threshold = HYSTERESIS * total;

    let title = `SIMULATING ${route.vehicleId || 'VEHICLE'}`;
    let message = '';

    if (simProgress >= 1) {
      message = '✅ TRIP COMPLETED';
      setDisplayStatus({ title, message });
      return;
    }

    if (stopDistances.length === 0) {
      message = '⏳ NO STOP DATA';
      setDisplayStatus({ title, message });
      return;
    }

    let prevStop = null;
    let nextStop = null;
    for (let i = 0; i < stopDistances.length; i++) {
      if (stopDistances[i].distance <= currentDist + 0.001) { 
        prevStop = stopDistances[i];
      } else {
        nextStop = stopDistances[i];
        break;
      }
    }

    if (nextStop && (nextStop.distance - currentDist) <= threshold && currentDist < nextStop.distance) {
      if (nextStop.type === 'pickup') {
        message = `📍 REACHED ${nextStop.id || 'EMPLOYEE'}`;
      } else if (nextStop.type === 'drop') {
        message = `📍 REACHED DROP`;
      } else if (nextStop.type === 'start') {
        message = `🏁 AT DEPOT`;
      }
    } 
    else if (prevStop) {
      if (prevStop.type === 'start') {
        if (nextStop) {
          message = `➡️ EN ROUTE TO ${nextStop.id || 'EMPLOYEE'}`;
        } else {
          message = `🏁 AT DEPOT`;
        }
      } 
      else if (prevStop.type === 'pickup') {
        if ((currentDist - prevStop.distance) <= threshold) {
          message = `🟢 PICKED ${prevStop.id || 'EMPLOYEE'}`;
        } else {
          if (nextStop) {
            if (nextStop.type === 'pickup') {
              message = `➡️ EN ROUTE TO ${nextStop.id}`;
            } else if (nextStop.type === 'drop') {
              message = `⬇️ GOING TO DROP`;
            } else {
              message = `➡️ EN ROUTE`;
            }
          } else {
            message = `➡️ GOING TO DROP`;
          }
        }
      } 
      else if (prevStop.type === 'drop') {
        if ((currentDist - prevStop.distance) <= threshold) {
          message = `🔴 DROPPED ${prevStop.id || 'LOCATION'}`;
        } else if (currentDist < total) {
          if (nextStop) {
            message = `➡️ EN ROUTE TO ${nextStop.id}`;
          } else {
            message = `✅ GOING TO DROP`;
          }
        } else {
          message = `✅ TRIP COMPLETED`;
        }
      }
    } else {
      message = `🏁 AT DEPOT`;
    }

    if (!message) message = `⏳ SIMULATING`;

    setDisplayStatus({ title, message });
  }, [simProgress, simulatingRoute, stopDistances]);

  const defaultCenter = dropoffs && dropoffs.length > 0 
    ? [dropoffs[0].lat, dropoffs[0].lng] 
    : [12.9716, 77.5946];

  if (!isMounted) return <div className="w-full h-full bg-slate-900 animate-pulse" />;

  const activeStops = selectedRouteIndex !== null 
      ? routes[selectedRouteIndex]?.stops || []
      : [];

  const pathColor = (selectedRouteIndex !== null && routes && routes[selectedRouteIndex]?.color) 
    ? routes[selectedRouteIndex].color 
    : '#22d3ee';

  return (
    <div className="w-full h-full bg-slate-900 relative">
      <MapContainer 
        key="unique-map-id" 
        center={defaultCenter} 
        zoom={13} 
        style={{ height: "100%", width: "100%" }}
        zoomControl={false}
      >
        <TileLayer
          attribution='&copy; OpenStreetMap'
          url="https://{s}.tile.openstreetmap.org/{z}/{x}/{y}.png"
        />

        {/* ROUTES */}
        {(routes || []).map((route, idx) => {
          const isSelected = selectedRouteIndex === idx;
          const isAnySelected = selectedRouteIndex !== null;
          const isDimmed = isAnySelected && !isSelected;
          const simProgressForRoute = (simulatingVehicleIndex === idx) ? simProgress : undefined;

          return (
            <React.Fragment key={`route-${idx}`}>
              <TraceRoute 
                route={route} 
                isSelected={isSelected} 
                isDimmed={isDimmed} 
                simulateProgress={simProgressForRoute}
              />
            </React.Fragment>
          );
        })}

        {selectedRouteIndex === null && (
          <>
            {routes?.map((route, idx) => {
              const depotStop = route.stops?.find(s => s.type === 'start');
              if (!depotStop || !depotIcon) return null;
              return (
                <Marker key={`depot-overview-${idx}`} position={[depotStop.lat, depotStop.lng]} icon={depotIcon}>
                  <Popup>
                    <div className="text-slate-900 font-bold">
                      Vehicle Depot<br/>
                      <span className="text-xs font-normal text-slate-600">{route.vehicleId}</span>
                      {depotStop.time && <div className="text-xs mt-1">Available from: {depotStop.time}</div>}
                    </div>
                  </Popup>
                </Marker>
              );
            })}
            {pickups?.map((p, i) => pickupIcon && (
              <Marker key={`pickup-overview-${i}`} position={[p.lat, p.lng]} icon={pickupIcon}>
                <Popup>
                  <div className="text-slate-900 font-bold">
                    Pickup Point<br/>
                    <span className="text-xs font-normal text-slate-600">Employee: {p.id}</span>
                  </div>
                </Popup>
              </Marker>
            ))}
            {dropoffs?.map((d, i) => dropIcon && (
              <Marker key={`drop-${i}`} position={[d.lat, d.lng]} icon={dropIcon}>
                <Popup><div className="text-slate-900 font-bold">Office / Drop Location</div></Popup>
              </Marker>
            ))}
          </>
        )}

        {selectedRouteIndex !== null && (
          <>
            {activeStops.map((p, i) => {
              if (!p.lat || !p.lng) return null;
              if (p.type === 'drop' && dropIcon) {
                return (
                  <Marker key={`active-drop-${i}`} position={[p.lat, p.lng]} icon={dropIcon}>
                    <Popup><div className="text-slate-900 font-bold">Drop Location<br/><span className="text-xs">ID: {p.id}</span></div></Popup>
                  </Marker>
                );
              }
              if (p.type === 'start' && depotIcon) {
                return (
                  <Marker key={`start-depot-${i}`} position={[p.lat, p.lng]} icon={depotIcon}>
                    <Popup>
                      <div className="text-slate-900 font-bold">
                        Vehicle Depot<br/><span className="text-xs">Start Point</span>
                        {p.time && <div className="text-xs mt-1">Available from: {p.time}</div>}
                      </div>
                    </Popup>
                  </Marker>
                );
              }
              if (p.type === 'pickup' && pickupIcon) {
                return (
                  <Marker key={`stop-${i}`} position={[p.lat, p.lng]} icon={pickupIcon}>
                    <Popup>
                      <div className="text-slate-900 font-bold">
                        Pickup Point<br />
                        <span className="text-xs">Employee: {p.id}</span>
                        {p.time && <div className="text-xs mt-1">Pickup time: {p.time}</div>}
                      </div>
                    </Popup>
                  </Marker>
                );
              }
              return null;
            })}
          </>
        )}

        <MapController 
          points={pickups} 
          dropoffs={dropoffs} 
          routes={routes} 
          selectedIndex={selectedRouteIndex} 
        />
      </MapContainer>
      
      {simulatingVehicleIndex !== null && displayStatus.message && (
        <div className="absolute top-6 right-6 z-[1000] min-w-[280px] max-w-sm">
          <div className="bg-black/90 backdrop-blur-md border border-cyan-500 rounded-2xl shadow-[0_0_20px_rgba(6,182,212,0.5)] p-5 text-cyan-300 font-mono">
            <div className="flex items-center justify-between border-b border-cyan-700 pb-2 mb-3">
              <div className="flex items-center gap-2">
                <span className="text-2xl">🌐</span>
                <span className="text-sm tracking-wider font-bold text-cyan-400">
                  {displayStatus.title}
                </span>
              </div>
              <div className="flex items-center gap-1">
                <span className="text-xl">➡️</span>
                <span className="text-xs uppercase tracking-wider text-cyan-500">ACTIVE</span>
              </div>
            </div>
            
            <div className="text-base font-semibold text-white flex items-center gap-2">
              <span className="text-cyan-400 text-xl">⚡</span>
              <span className="tracking-wide">{displayStatus.message}</span>
            </div>

            <div className="mt-3 h-px w-full bg-gradient-to-r from-transparent via-cyan-500 to-transparent" />
          </div>
        </div>
      )}

      <div className="absolute bottom-6 left-6 bg-white/95 backdrop-blur-sm p-4 rounded-lg shadow-2xl z-[1000] text-xs font-sans border border-slate-300 text-slate-800">
         <div className="font-bold mb-3 text-slate-900 border-b border-slate-200 pb-1">LIVE FEED LEGEND</div>
         <div className="flex items-center gap-3 mb-2">
            {pickupIcon && <img src={pickupIcon.options.iconUrl} className="h-5 w-auto" alt="Pickup"/>}
            <span>Employee Pickup</span>
         </div>
         <div className="flex items-center gap-3 mb-2">
            {dropIcon && <img src={dropIcon.options.iconUrl} className="h-5 w-auto" alt="Drop"/>}
            <span>Office / Drop</span>
         </div>
         <div className="flex items-center gap-3 mb-2">
            {depotIcon && <img src={depotIcon.options.iconUrl} className="h-5 w-auto" alt="Depot"/>}
            <span>Vehicle Depot</span>
         </div>
         <div className="flex items-center gap-3 mb-2">
            {carIcon && <img src={carIcon.options.iconUrl} className="h-5 w-auto" alt="Car"/>}
            <span>Fleet Vehicle</span>
         </div>
         <div className="flex items-center gap-3">
            <div className="w-6 h-1 rounded-full" style={{ backgroundColor: pathColor }}></div> 
            <span>Optimal Path</span>
         </div>
      </div>
    </div>
  );
}