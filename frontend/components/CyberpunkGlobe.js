"use client";

import React, { useEffect, useState, useRef } from "react";
import dynamic from "next/dynamic";

if (typeof window !== "undefined" && typeof window.GPUShaderStage === "undefined") {
  window.GPUShaderStage = { VERTEX: 1, FRAGMENT: 2, COMPUTE: 4 };
}

const Globe = dynamic(() => import("react-globe.gl"), { 
  ssr: false 
});
const CAR_SPEED = 0.001; 
const TOTAL_CARS = 15;   
const TAXI_COLOR = "#fbbf24"; 

const ROUTE_COLORS = [
  "rgba(34, 211, 238, 0.6)", 
  "rgba(192, 132, 252, 0.6)",
  "rgba(219, 39, 119, 0.6)",
  "rgba(250, 204, 21, 0.6)" 
];

const generateRandomRoutes = (count) => {
  return [...Array(count).keys()].map((i) => ({
    id: i,
    startLat: (Math.random() - 0.5) * 150, 
    startLng: (Math.random() - 0.5) * 360,
    endLat: (Math.random() - 0.5) * 150,
    endLng: (Math.random() - 0.5) * 360,
    routeColor: ROUTE_COLORS[Math.floor(Math.random() * ROUTE_COLORS.length)]
  }));
};

function getIntermediatePoint(startLat, startLng, endLat, endLng, f) {
  const toRad = (d) => (d * Math.PI) / 180;
  const toDeg = (r) => (r * 180) / Math.PI;

  const lat1 = toRad(startLat);
  const lon1 = toRad(startLng);
  const lat2 = toRad(endLat);
  const lon2 = toRad(endLng);

  const d = 2 * Math.asin(Math.sqrt(Math.pow(Math.sin((lat2 - lat1) / 2), 2) +
    Math.cos(lat1) * Math.cos(lat2) * Math.pow(Math.sin((lon2 - lon1) / 2), 2)));
  
  const A = Math.sin((1 - f) * d) / Math.sin(d);
  const B = Math.sin(f * d) / Math.sin(d);

  const x = A * Math.cos(lat1) * Math.cos(lon1) + B * Math.cos(lat2) * Math.cos(lon2);
  const y = A * Math.cos(lat1) * Math.sin(lon1) + B * Math.cos(lat2) * Math.sin(lon2);
  const z = A * Math.sin(lat1) + B * Math.sin(lat2);

  const lat = Math.atan2(z, Math.sqrt(x * x + y * y));
  const lon = Math.atan2(y, x);

  return [toDeg(lat), toDeg(lon)];
}

function getBearing(startLat, startLng, endLat, endLng) {
  const toRad = (d) => (d * Math.PI) / 180;
  const toDeg = (r) => (r * 180) / Math.PI;

  const lat1 = toRad(startLat);
  const lat2 = toRad(endLat);
  const dLon = toRad(endLng - startLng);

  const y = Math.sin(dLon) * Math.cos(lat2);
  const x = Math.cos(lat1) * Math.sin(lat2) -
            Math.sin(lat1) * Math.cos(lat2) * Math.cos(dLon);
  
  const brng = toDeg(Math.atan2(y, x));
  return (brng + 360) % 360;
}

export default function CyberpunkGlobe() {
  const globeEl = useRef();
  const [mounted, setMounted] = useState(false);
  const [routes] = useState(() => generateRandomRoutes(TOTAL_CARS));
  
  const [cars, setCars] = useState(
    routes.map((route) => ({
      ...route,
      lat: route.startLat,
      lng: route.startLng,
      rotation: 0,
      progress: Math.random(), 
    }))
  );

  useEffect(() => {
    setMounted(true);
    if (globeEl.current) {
      globeEl.current.controls().autoRotate = true;
      globeEl.current.controls().autoRotateSpeed = 0.5;
      globeEl.current.controls().enableZoom = false;
      globeEl.current.pointOfView({ altitude: 1.8 });
    }
  }, []);

  useEffect(() => {
    let animationFrameId;

    const animate = () => {
      setCars(prevCars => 
        prevCars.map(car => {
          let newProgress = car.progress + CAR_SPEED;
          if (newProgress >= 1) newProgress = 0;

          const { startLat, startLng, endLat, endLng } = car;
          const [currentLat, currentLng] = getIntermediatePoint(startLat, startLng, endLat, endLng, newProgress);
          const [nextLat, nextLng] = getIntermediatePoint(startLat, startLng, endLat, endLng, newProgress + 0.05);
          const rotation = getBearing(currentLat, currentLng, nextLat, nextLng);

          return { ...car, progress: newProgress, lat: currentLat, lng: currentLng, rotation };
        })
      );
      animationFrameId = requestAnimationFrame(animate);
    };

    animate();
    return () => cancelAnimationFrame(animationFrameId);
  }, []);

  if (!mounted) return null; 

  return (
    <div className="absolute inset-0 z-0 flex items-center justify-center bg-slate-900">
      <Globe
        ref={globeEl}
        globeImageUrl="//unpkg.com/three-globe/example/img/earth-night.jpg"
        backgroundImageUrl="//unpkg.com/three-globe/example/img/night-sky.png"
        atmosphereColor="#3b82f6" 
        atmosphereAltitude={0.15}

        htmlElementsData={cars}
        htmlLat="lat"
        htmlLng="lng"
        
        htmlAltitude={0} 
        
        htmlElement={(d) => {
          const el = document.createElement('div');
          el.innerHTML = `
            <div style="
              transform: translate(-50%, -50%) rotate(${d.rotation}deg);
              width: 32px; height: 32px;
              display: flex; align-items: center; justify-content: center;
              pointer-events: none;
            ">
              <svg xmlns="http://www.w3.org/2000/svg" viewBox="0 0 100 100" 
                style="width: 100%; height: 100%; filter: drop-shadow(0 0 2px rgba(0,0,0,0.9));">
                
                <rect x="30" y="10" width="40" height="80" rx="10" fill="${TAXI_COLOR}" stroke="#eab308" stroke-width="2"/>
                
                <path d="M 35 25 Q 50 20 65 25 L 65 35 L 35 35 Z" fill="#1e293b" />
                <path d="M 35 75 Q 50 80 65 75 L 65 65 L 35 65 Z" fill="#1e293b" />
                
                <rect x="40" y="45" width="20" height="10" fill="#facc15" stroke="#000" stroke-width="1"/>
                
                <circle cx="35" cy="10" r="3" fill="#fff" />
                <circle cx="65" cy="10" r="3" fill="#fff" />
                <circle cx="35" cy="90" r="3" fill="#ef4444" />
                <circle cx="65" cy="90" r="3" fill="#ef4444" />
              </svg>
            </div>
          `;
          return el;
        }}

        arcsData={routes}
        arcColor="routeColor" 
        arcDashLength={1} 
        arcStroke={0.5}   
        arcDashGap={0}    
      />
      <div className="absolute inset-0 bg-slate-900/10 pointer-events-none z-10" />
    </div>
  );
}