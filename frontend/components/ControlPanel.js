"use client";
import React, { useEffect, useState, useRef } from "react";
import {
  Settings,
  Zap,
  RefreshCw,
  FileSpreadsheet,
  Upload,
  AlertCircle,
  Terminal,
  Users,
  Download,
} from "lucide-react";
import * as XLSX from "xlsx";
import axios from "axios";

const API_ENDPOINT = "/api/backend/upload";
const OSRM_BASE_URL = "https://router.project-osrm.org/route/v1/driving";

const getVehicleColor = (index) => {
  const colors = [
    "#22d3ee",
    "#a855f7",
    "#f472b6",
    "#fbbf24",
    "#34d399",
    "#f87171",
  ];
  return colors[index % colors.length];
};

const excelTimeToHHMMSS = (serial) => {
  if (typeof serial === "string") return serial;
  if (typeof serial !== "number") return "08:00:00";
  const totalSeconds = Math.round(serial * 24 * 3600);
  const hours = Math.floor(totalSeconds / 3600);
  const minutes = Math.floor((totalSeconds % 3600) / 60);
  const seconds = totalSeconds % 60;
  return `${hours.toString().padStart(2, "0")}:${minutes.toString().padStart(2, "0")}:${seconds.toString().padStart(2, "0")}`;
};

const timeToMinutes = (timeStr) => {
  if (!timeStr) return 0;
  const parts = timeStr.split(":").map(Number);
  return parts[0] * 60 + (parts[1] || 0);
};

const minutesToDurationText = (totalMins) => {
  if (totalMins < 0) return "0 min";
  const hours = Math.floor(totalMins / 60);
  const mins = totalMins % 60;
  if (hours > 0) return `${hours}h ${mins}m`;
  return `${mins} min`;
};

const haversineDistance = (lat1, lon1, lat2, lon2) => {
  const R = 6371;
  const dLat = ((lat2 - lat1) * Math.PI) / 180;
  const dLon = ((lon2 - lon1) * Math.PI) / 180;
  const a =
    Math.sin(dLat / 2) * Math.sin(dLat / 2) +
    Math.cos((lat1 * Math.PI) / 180) *
    Math.cos((lat2 * Math.PI) / 180) *
    Math.sin(dLon / 2) *
    Math.sin(dLon / 2);
  const c = 2 * Math.atan2(Math.sqrt(a), Math.sqrt(1 - a));
  return R * c;
};

const computeHaversineDistance = (waypoints) => {
  let total = 0;
  for (let i = 1; i < waypoints.length; i++) {
    const [lat1, lng1] = waypoints[i - 1];
    const [lat2, lng2] = waypoints[i];
    total += haversineDistance(lat1, lng1, lat2, lng2);
  }
  return total;
};



export default function ControlPanel({
  setResult,
  file,
  optimizationLevel,
  onDataGenerated,
  onReupload,
}) {
  const [loading, setLoading] = useState(false);
  const [stats, setStats] = useState(null);
  const [statusMsg, setStatusMsg] = useState("Waiting...");
  const [errorMsg, setErrorMsg] = useState(null);
  const [debugData, setDebugData] = useState(null);
  const [outputCsv, setOutputCsv] = useState(null);

  const lastProcessedFile = useRef(null);

  const handleDownloadCSV = () => {
    if (!outputCsv) return;

    // ---- First CSV (existing one) ----
    const blob1 = new Blob([outputCsv], { type: "text/csv;charset=utf-8;" });
    const url1 = URL.createObjectURL(blob1);

    const link1 = document.createElement("a");
    link1.href = url1;
    link1.setAttribute("download", "velora_optimized_routes.csv");
    document.body.appendChild(link1);
    link1.click();
    document.body.removeChild(link1);

    // ---- Second CSV (employee CSV without first line) ----
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

  useEffect(() => {
    if (file && file !== lastProcessedFile.current) {
      handleUploadAndOptimize(file);
    }
  }, [file]);

  const sheetToCSVBlob = (workbook, sheetName) => {
    if (!sheetName) return null;
    const sheet = workbook.Sheets[sheetName];
    const csv = XLSX.utils.sheet_to_csv(sheet, {
      FS: ",",
      RS: "\n",
      blankrows: false,
    });
    return new Blob([csv], { type: "text/csv" });
  };

  const findSheet = (names, target) => {
    return names.find((n) => n.toLowerCase().includes(target.toLowerCase()));
  };

  const findValue = (row, aliases) => {
    const keys = Object.keys(row);
    for (let alias of aliases) {
      const cleanAlias = alias.toLowerCase().replace(/[\s_]/g, "");
      const key = keys.find(
        (k) => k.toLowerCase().replace(/[\s_]/g, "") === cleanAlias,
      );
      if (key) return row[key];
    }
    return null;
  };

  const fetchRealRoadPath = async (waypoints, forceHaversine = false) => {
    if (waypoints.length < 2)
      return { path: waypoints, distance: 0, success: true };

    if (forceHaversine) {
      const haversineDist = computeHaversineDistance(waypoints);
      return {
        path: waypoints,
        distance: haversineDist.toFixed(1),
        success: true,
      };
    }

    const coordString = waypoints.map((p) => `${p[1]},${p[0]}`).join(";");
    try {
      const url = `${OSRM_BASE_URL}/${coordString}?overview=full&geometries=geojson&continue_straight=true`;
      const res = await fetch(url);
      if (!res.ok) throw new Error(`OSRM HTTP ${res.status}`);
      const data = await res.json();
      if (data.code === "Ok" && data.routes?.length > 0) {
        const route = data.routes[0];
        return {
          path: route.geometry.coordinates.map((c) => [c[1], c[0]]),
          distance: (route.distance / 1000).toFixed(1),
          success: true,
        };
      }
    } catch (e) {
      console.warn("OSRM failed", e);
    }
    return { success: false };
  };

  const buildRouteForVehicle = async (vId, stops, vDetails, useHaversine) => {
    let startMin = stops.length > 0 ? timeToMinutes(stops[0].time) : 0;
    let endMin =
      stops.length > 0 ? timeToMinutes(stops[stops.length - 1].time) : 0;
    const durationMinutes = endMin - startMin;

    if (vDetails.lat && vDetails.lng) {
      stops.unshift({
        type: "start",
        time: vDetails.available_from || "08:00:00",
        id: "DEPOT",
        lat: vDetails.lat,
        lng: vDetails.lng,
      });
      startMin = timeToMinutes(vDetails.available_from || "08:00:00");
    }

    const waypoints = stops.map((s) => [s.lat, s.lng]);
    const roadData = await fetchRealRoadPath(waypoints, useHaversine);

    const distanceNum = parseFloat(roadData.distance) || 0;
    const passengers = stops
      .filter((s) => s.type === "pickup")
      .map((s) => s.id);

    return {
      vehicleId: vId,
      stops: stops.map((s, i) => ({
        lat: s.lat,
        lng: s.lng,
        sequence: i,
        type: s.type,
        id: s.id,
        time: s.time,
      })),
      path: roadData.path,
      distance: distanceNum ? `${distanceNum} km` : "0 km",
      distanceNum,
      duration: minutesToDurationText(durationMinutes),
      durationMinutes,
      passengers,
      startTime: stops[0]?.time || "08:00",
      vehicleType: vDetails.type || "normal",
      propulsion: vDetails.fuel || "Electric",
      capacity: vDetails.capacity || 4,
      occupancy: passengers.length,
      success: roadData.success,
    };
  };

  const handleUploadAndOptimize = async (uploadedFile) => {
    setLoading(true);
    setStatusMsg("Reading Excel...");
    setErrorMsg(null);
    setDebugData(null);
    setOutputCsv(null);

    lastProcessedFile.current = uploadedFile;

    const reader = new FileReader();

    reader.onload = async (e) => {
      try {
        console.log("📂 Excel Loaded");

        const excelData = e.target.result;
        const workbook = XLSX.read(excelData, { type: "array" });
        const sheetNames = workbook.SheetNames;
        console.log("📑 Sheet names:", sheetNames);

        const empSheet =
          findSheet(sheetNames, "employee") || findSheet(sheetNames, "request");
        const vehSheet =
          findSheet(sheetNames, "vehicle") || findSheet(sheetNames, "fleet");
        const metaSheet =
          findSheet(sheetNames, "metadata") || findSheet(sheetNames, "config");
        const baselineSheet =
          findSheet(sheetNames, "baseline") || findSheet(sheetNames, "base");

        console.log("📄 Found sheets:", {
          empSheet,
          vehSheet,
          metaSheet,
          baselineSheet,
        });

        if (!empSheet) throw new Error("Missing 'Employees' sheet.");

        const empRaw = XLSX.utils.sheet_to_json(workbook.Sheets[empSheet]);
        const employeeDetails = {};
        const locationMap = {};

        empRaw.forEach((row) => {
          const id =
            findValue(row, ["id", "employee", "employee_id"]) ||
            row.employee_id ||
            row.id ||
            row.ID;
          const pLat = parseFloat(
            findValue(row, [
              "pickup_lat",
              "pickuplatitude",
              "lat",
              "latitude",
            ]) ||
            row.pickup_lat ||
            row.lat ||
            row.Lat,
          );
          const pLng = parseFloat(
            findValue(row, [
              "pickup_lng",
              "pickuplongitude",
              "lng",
              "longitude",
            ]) ||
            row.pickup_lng ||
            row.lng ||
            row.Lng,
          );
          const dLat = parseFloat(
            findValue(row, ["drop_lat", "droplatitude", "officelat"]) ||
            row.drop_lat ||
            row.drop_latitude,
          );
          const dLng = parseFloat(
            findValue(row, ["drop_lng", "droplongitude", "officelng"]) ||
            row.drop_lng ||
            row.drop_longitude,
          );
          if (id && pLat && pLng) {
            locationMap[id] = {
              pickup: [pLat, pLng],
              drop: dLat && dLng ? [dLat, dLng] : null,
            };

            const earliest =
              findValue(row, [
                "earliest_pickup",
                "pickup_start",
                "start_time",
              ]) || row.earliest_pickup;
            const latest =
              findValue(row, ["latest_drop", "drop_end", "end_time"]) ||
              row.latest_drop;
            const vehPref =
              findValue(row, ["vehicle_preference", "vehicle_type", "pref"]) ||
              row.vehicle_preference ||
              "any";
            const sharePref =
              findValue(row, ["sharing_preference", "sharing"]) ||
              row.sharing_preference ||
              "single";

            employeeDetails[id] = {
              pickup: [pLat, pLng],
              drop: dLat && dLng ? [dLat, dLng] : null,
              earliest_pickup: earliest
                ? timeToMinutes(excelTimeToHHMMSS(earliest))
                : 0,
              latest_drop: latest
                ? timeToMinutes(excelTimeToHHMMSS(latest))
                : 24 * 60,
              vehicle_preference: (vehPref || "any").toString().toLowerCase(),
              sharing_preference: (sharePref || "single")
                .toString()
                .toLowerCase(),
              priority: parseFloat(
                findValue(row, ["priority"]) || row.priority || 1,
              ),
            };
          }
        });

        const vehicleDetailsMap = {};
        if (vehSheet) {
          const vehRaw = XLSX.utils.sheet_to_json(workbook.Sheets[vehSheet]);
          vehRaw.forEach((row) => {
            const vId =
              findValue(row, ["vehicle", "vehicle_id", "id"]) ||
              row.vehicle_id ||
              row.id ||
              row.ID;
            const lat = parseFloat(
              findValue(row, ["current_lat", "start_lat", "lat"]) ||
              row.current_lat ||
              row.start_lat,
            );
            const lng = parseFloat(
              findValue(row, ["current_lng", "start_lng", "lng"]) ||
              row.current_lng ||
              row.start_lng,
            );
            let available_from =
              findValue(row, ["available_from", "start_time"]) ||
              row.available_from;
            const type =
              findValue(row, ["type", "category", "class", "vehicle_type"]) ||
              row.category ||
              row.vehicle_type ||
              "Normal";
            const fuel =
              findValue(row, ["fuel", "propulsion", "engine", "fuel_type"]) ||
              row.fuel_type ||
              "Electric";
            const capacity =
              parseInt(
                findValue(row, ["capacity", "seats", "max_passengers"]) ||
                row.capacity ||
                4,
              ) || 4;
            const costPerKm = parseFloat(
              findValue(row, ["cost_per_km", "cost", "price_per_km"]) ||
              row.cost_per_km ||
              10,
            );

            if (typeof available_from === "number")
              available_from = excelTimeToHHMMSS(available_from);

            if (vId) {
              vehicleDetailsMap[vId] = {
                lat: !isNaN(lat) ? lat : null,
                lng: !isNaN(lng) ? lng : null,
                available_from: available_from || "08:00:00",
                type: type.toString().toLowerCase(),
                fuel,
                capacity,
                cost_per_km: costPerKm,
              };
            }
          });
        }

        let allowExternalMaps = true;
        const priorityDelays = {};
        if (metaSheet) {
          console.log(" Parsing metadata sheet:", metaSheet);
          const metaRaw = XLSX.utils.sheet_to_json(workbook.Sheets[metaSheet]);
          console.log(" Metadata raw rows:", metaRaw);
          const metaDict = {};
          metaRaw.forEach((row) => {
            const key = row.key;
            const val = row.value;
            if (key && val !== undefined) {
              metaDict[key.trim()] = val;
            }
          });
          console.log(" Metadata dictionary keys:", Object.keys(metaDict));

          if (metaDict.allow_external_maps !== undefined) {
            const val = metaDict.allow_external_maps;
            if (typeof val === "string") {
              allowExternalMaps = val.toLowerCase() === "true";
            } else {
              allowExternalMaps = val === true;
            }
            console.log(
              " allow_external_maps =",
              allowExternalMaps,
              "(raw:",
              val,
              ")",
            );
          } else {
            console.log(
              " allow_external_maps key not found in metadata, using default true",
            );
          }

          for (let p = 1; p <= 5; p++) {
            const key = `priority_${p}_max_delay_min`;
            if (metaDict[key] !== undefined) {
              const val = metaDict[key];
              const num = parseFloat(val);
              if (!isNaN(num)) {
                priorityDelays[p] = num;
                console.log(` priority_${p}_max_delay_min = ${num}`);
              }
            }
          }
        } else {
          console.log(
            " No metadata sheet found, using default allowExternalMaps = true and no priority delays",
          );
        }

        const baselineMap = {};
        if (baselineSheet) {
          const baseRaw = XLSX.utils.sheet_to_json(
            workbook.Sheets[baselineSheet],
          );
          baseRaw.forEach((row) => {
            const empId =
              findValue(row, ["employee_id", "employee", "id"]) ||
              row.employee_id ||
              row.id;
            const cost = parseFloat(
              findValue(row, ["baseline_cost", "cost"]) || row.baseline_cost,
            );
            const time = parseFloat(
              findValue(row, ["baseline_time_min", "time_min"]) ||
              row.baseline_time_min,
            );
            if (empId) {
              baselineMap[empId] = { cost, time };
            }
          });
        }

        const formData = new FormData();
        formData.append(
          "employees",
          sheetToCSVBlob(workbook, empSheet),
          "employees.csv",
        );
        if (vehSheet)
          formData.append(
            "vehicles",
            sheetToCSVBlob(workbook, vehSheet),
            "vehicles.csv",
          );
        if (metaSheet)
          formData.append(
            "metadata",
            sheetToCSVBlob(workbook, metaSheet),
            "metadata.csv",
          );
        if (baselineSheet)
          formData.append(
            "basedata",
            sheetToCSVBlob(workbook, baselineSheet),
            "baseline.csv",
          );

        let backendOptValue = 60;
        if (optimizationLevel === "ultra_fast") backendOptValue = 10;
        if (optimizationLevel === "fast") backendOptValue = 20;

        formData.append("optimizationLevel", backendOptValue);

        setStatusMsg("Optimizing (Backend)...");
        const response = await axios.post(API_ENDPOINT, formData, {
          headers: { "Content-Type": "multipart/form-data" },
        });

        const resultRaw = response.data;
        let result = resultRaw;

        if (typeof resultRaw === "string") {
          try {
            result = JSON.parse(resultRaw);
          } catch (e) { }
        }
        setResult(result);
        setDebugData(
          typeof result === "object" ? JSON.stringify(result, null, 2) : result,
        );
        let costSave = result.results.mem.csv_employee.split("\n")[0].split(",")[0];
        let timeSave = result.results.mem.csv_employee.split("\n")[0].split(",")[1];
        let rawCsv = "";
        if (typeof result === "string" && result.includes("vehicle_id")) {
          rawCsv = result;
        } else if (result?.results?.mem?.csv_vehicle) {

          rawCsv = result.results.mem.csv_vehicle;
          console.log(" Using memetic algorithm output");
        } else if (result?.results?.ALNS?.csv_vehicle) {
          rawCsv = result.results.ALNS.csv_vehicle;
          console.log("Using ALNS output");
        } else if (result?.csv_vehicle) {
          rawCsv = result.csv_vehicle;
          console.log(" Using raw CSV");
        }

        if (!rawCsv)
          throw new Error(
            "Could not find routing data in the server response.",
          );

        rawCsv = rawCsv.replace(/^\uFEFF/, "").replace(/"/g, "");
        const lines = rawCsv
          .trim()
          .split(/\r?\n/)
          .filter((line) => line.trim() !== "");

        let totalScore = 0;
        const firstLine = lines[0];
        if (firstLine && !firstLine.toLowerCase().includes("vehicle_id")) {
          const parts = firstLine.split(",").map(Number);
          if (parts.length >= 1 && !isNaN(parts[0])) {
            totalScore = parts[0];
          }
          lines.shift();
        }

        const headerIdx = lines.findIndex(
          (line) =>
            line.toLowerCase().includes("vehicle_id") ||
            line.toLowerCase().includes("employee_id"),
        );
        if (headerIdx === -1)
          throw new Error("Could not find headers in the backend response.");

        const cleanCsvString = lines.slice(headerIdx).join('\n');

        const headers = lines[headerIdx]
          .split(",")
          .map((h) => h.trim().toLowerCase());

        const assignments = lines
          .slice(headerIdx + 1)
          .map((line) => {
            const values = line.split(",");
            const obj = {};
            headers.forEach((h, i) => {
              if (h) obj[h] = values[i]?.trim() || "";
            });
            return obj;
          })
          .filter((row) => row.vehicle_id && row.employee_id);

        if (assignments.length === 0)
          throw new Error("Failed to extract valid assignment rows.");

        console.log("📋 Raw assignments from CSV:");
        assignments.forEach(a => {
          console.log(`  ${a.vehicle_id} ${a.employee_id} pickup:${a.pickup_time} drop:${a.drop_time}`);
        });

        const superEmployeeMap = {};
        Object.entries(employeeDetails).forEach(([id, det]) => {
          const strId = String(id).trim().toLowerCase();
          superEmployeeMap[strId] = det;
          const stripped = strId.replace(/^[a-z0]+/i, "");
          if (stripped) superEmployeeMap[stripped] = det;
        });

        const vehicleGroups = {};
        const vehicleInfo = {};

        assignments.forEach((row) => {
          const vId = row.vehicle_id;
          const vDetails = vehicleDetailsMap[vId] || {
            type: row.category || "normal",
            fuel: "Electric",
            capacity: 4,
            cost_per_km: 10,
          };

          if (!vehicleGroups[vId]) {
            vehicleGroups[vId] = [];
            vehicleInfo[vId] = vDetails;
          }

          const empId = String(row.employee_id).trim().toLowerCase();
          const strippedEmpId = empId.replace(/^[a-z0]+/i, "");
          const empDet =
            superEmployeeMap[empId] || superEmployeeMap[strippedEmpId];

          if (empDet && empDet.pickup) {
            vehicleGroups[vId].push({
              type: "pickup",
              time: row.pickup_time,
              id: String(row.employee_id).toUpperCase(),
              lat: empDet.pickup[0],
              lng: empDet.pickup[1],
            });
            if (empDet.drop) {
              const last = vehicleGroups[vId][vehicleGroups[vId].length - 1];
              if (
                !(
                  last &&
                  last.lat === empDet.drop[0] &&
                  last.lng === empDet.drop[1]
                )
              ) {
                vehicleGroups[vId].push({
                  type: "drop",
                  time: row.drop_time,
                  id: "OFFICE",
                  lat: empDet.drop[0],
                  lng: empDet.drop[1],
                });
              }
            }
          }
        });

        setStatusMsg(
          allowExternalMaps
            ? "Requesting OSRM routes..."
            : "External maps disabled, using haversine...",
        );
        const vehicleIds = Object.keys(vehicleGroups);
        let parsedRoutes = [];

        for (let index = 0; index < vehicleIds.length; index++) {
          const vId = vehicleIds[index];
          const originalStops = [...vehicleGroups[vId]];
          console.log(`🚗 Vehicle ${vId} stops BEFORE sorting:`,
            originalStops.map(s => `${s.time} ${s.type} ${s.id}`).join(' → ')
          );

          let stops = originalStops.sort((a, b) => a.time.localeCompare(b.time));

          console.log(`🚗 Vehicle ${vId} stops AFTER sorting:`,
            stops.map(s => `${s.time} ${s.type} ${s.id}`).join(' → ')
          );

          const vDetails = vehicleInfo[vId] || {};

          let startMin = stops.length > 0 ? timeToMinutes(stops[0].time) : 0;
          let endMin =
            stops.length > 0 ? timeToMinutes(stops[stops.length - 1].time) : 0;
          const durationMinutes = endMin - startMin;

          if (vDetails.lat && vDetails.lng) {
            stops.unshift({
              type: "start",
              time: vDetails.available_from || "08:00:00",
              id: "DEPOT",
              lat: vDetails.lat,
              lng: vDetails.lng,
            });
            startMin = timeToMinutes(vDetails.available_from || "08:00:00");
          }

          const waypoints = stops.map((s) => [s.lat, s.lng]);
          const roadData = await fetchRealRoadPath(waypoints, allowExternalMaps ? false : true);

          const distanceNum = parseFloat(roadData.distance) || 0;
          const passengers = stops
            .filter((s) => s.type === "pickup")
            .map((s) => s.id);

          const route = {
            vehicleId: vId,
            stops: stops.map((s, i) => ({
              lat: s.lat,
              lng: s.lng,
              sequence: i,
              type: s.type,
              id: s.id,
              time: s.time,
            })),
            path: roadData.path,
            distance: distanceNum ? `${distanceNum} km` : "0 km",
            distanceNum,
            duration: minutesToDurationText(durationMinutes),
            durationMinutes,
            passengers,
            startTime: stops[0]?.time || "08:00",
            vehicleType: vDetails.type || "normal",
            propulsion: vDetails.fuel || "Electric",
            capacity: vDetails.capacity || 4,
            occupancy: passengers.length,
            success: roadData.success,
          };

          parsedRoutes.push(route);
          console.log(`✅ Final route for ${vId}:`,
            route.stops.map(s => `${s.time} ${s.type} ${s.id}`).join(' → ')
          );
        }

        parsedRoutes = parsedRoutes.map((route, index) => ({
          ...route,
          color: getVehicleColor(index),
        }));

        const employeeTimeMap = {};
        assignments.forEach((row) => {
          const empId = String(row.employee_id).trim().toLowerCase();
          const pickup = timeToMinutes(row.pickup_time);
          const drop = timeToMinutes(row.drop_time);
          const optimizedTime = drop - pickup;
          employeeTimeMap[empId] = optimizedTime;
        });

        const employeeTimeComparison = [];
        Object.keys(baselineMap).forEach((empId) => {
          const empIdLower = empId.toLowerCase();
          const stripped = empIdLower.replace(/^[a-z0]+/i, "");
          let matchedId = empIdLower;
          if (!employeeTimeMap[empIdLower] && employeeTimeMap[stripped])
            matchedId = stripped;
          const optimized = employeeTimeMap[matchedId];
          if (optimized !== undefined) {
            employeeTimeComparison.push({
              employee_id: empId,
              baselineTime: baselineMap[empId].time || 0,
              optimizedTime: optimized,
            });
          }
        });

        const vehicleEvents = {};
        parsedRoutes.forEach((route) => {
          const events = [];
          route.stops.forEach((stop) => {
            if (stop.type === "pickup") {
              events.push({ time: timeToMinutes(stop.time), delta: 1 });
            } else if (stop.type === "drop") {
              events.push({ time: timeToMinutes(stop.time), delta: -1 });
            }
          });
          events.sort((a, b) => a.time - b.time);
          vehicleEvents[route.vehicleId] = events;
        });

        const employeeMaxOccupancy = {};
        assignments.forEach((row) => {
          const empId = String(row.employee_id).trim().toLowerCase();
          const stripped = empId.replace(/^[a-z0]+/i, "");
          const empDet = superEmployeeMap[empId] || superEmployeeMap[stripped];
          if (!empDet) return;

          const vehicleId = row.vehicle_id;
          const events = vehicleEvents[vehicleId];
          if (!events) return;

          const pickupTime = timeToMinutes(row.pickup_time);
          const dropTime = timeToMinutes(row.drop_time);

          let occ = 0;
          for (let e of events) {
            if (e.time < pickupTime) occ += e.delta;
            else break;
          }
          let maxOcc = occ;
          for (let e of events) {
            if (e.time >= pickupTime && e.time <= dropTime) {
              occ += e.delta;
              if (occ > maxOcc) maxOcc = occ;
            }
            if (e.time > dropTime) break;
          }
          employeeMaxOccupancy[empId] = maxOcc;
          if (stripped) employeeMaxOccupancy[stripped] = maxOcc;
        });

        const totalEmployees = Object.keys(employeeDetails).length;
        const totalVehiclesUsed = parsedRoutes.filter(
          (r) => r.occupancy > 0,
        ).length;
        const totalVehiclesAvailable =
          Object.keys(vehicleDetailsMap).length || totalVehiclesUsed;

        const assignedVehicleIds = new Set(
          parsedRoutes.map((r) => r.vehicleId),
        );
        const unassignedVehicleIds = Object.keys(vehicleDetailsMap).filter(
          (id) => !assignedVehicleIds.has(id),
        );

        let totalDistance = 0;
        let totalOptimizedTime = 0;
        let totalBaselineCost = 0;
        let totalBaselineTime = 0;
        let totalOptimizedCostFromDistance = 0;

        const perVehicleCostData = [];

        parsedRoutes.forEach((route) => {
          totalDistance += route.distanceNum;
          totalOptimizedTime += route.durationMinutes;

          const vCostPerKm = vehicleInfo[route.vehicleId]?.cost_per_km || 10;
          const optCost = route.distanceNum * vCostPerKm;
          totalOptimizedCostFromDistance += optCost;

          let vehBaselineCost = 0;
          let vehBaselineTime = 0;
          route.passengers.forEach((empId) => {
            const empIdLower = empId.toLowerCase();
            const stripped = empIdLower.replace(/^[a-z0]+/i, "");
            const base =
              baselineMap[empId] ||
              baselineMap[empIdLower] ||
              baselineMap[stripped];
            if (base) {
              vehBaselineCost += base.cost || 0;
              vehBaselineTime += base.time || 0;
            }
          });
          totalBaselineCost += vehBaselineCost;
          totalBaselineTime += vehBaselineTime;

          perVehicleCostData.push({
            name: route.vehicleId,
            baseline: vehBaselineCost,
            optimized: parseFloat(optCost.toFixed(1)),
          });
        });

        const isVehiclePrefSatisfied = (requested, assigned) => {
          if (requested === "any") return true;
          if (
            requested === "normal" &&
            (assigned === "normal" || assigned === "premium")
          )
            return true;
          if (requested === "premium" && assigned === "premium") return true;
          return false;
        };

        let vehPrefSatisfied = 0,
          sharePrefSatisfied = 0,
          timeWindowSatisfied = 0;
        const violations = [];

        assignments.forEach((row) => {
          const empId = String(row.employee_id).trim().toLowerCase();
          const stripped = empId.replace(/^[a-z0]+/i, "");
          const empDet = superEmployeeMap[empId] || superEmployeeMap[stripped];
          if (!empDet) return;

          const vehicle = parsedRoutes.find(
            (r) => r.vehicleId === row.vehicle_id,
          );
          if (!vehicle) return;

          const empPref = empDet.vehicle_preference;
          const vehType = vehicle.vehicleType;
          if (isVehiclePrefSatisfied(empPref, vehType)) {
            vehPrefSatisfied++;
          } else {
            violations.push({
              employee_id: row.employee_id,
              type: "Vehicle Preference",
              expected: empPref,
              actual: vehType,
            });
          }

          const sharePref = empDet.sharing_preference;
          const maxOcc =
            employeeMaxOccupancy[empId] ||
            employeeMaxOccupancy[stripped] ||
            vehicle.occupancy;
          let shareOk = false;
          if (sharePref === "single" && maxOcc <= 1) shareOk = true;
          else if (sharePref === "double" && maxOcc <= 2) shareOk = true;
          else if (sharePref === "triple" && maxOcc <= 3) shareOk = true;
          else if (sharePref === "any") shareOk = true;

          if (shareOk) {
            sharePrefSatisfied++;
          } else {
            violations.push({
              employee_id: row.employee_id,
              type: "Sharing Preference",
              expected: sharePref,
              actual: `${maxOcc} pax (max during trip)`,
            });
          }

          const pickupMin = timeToMinutes(row.pickup_time);
          const dropMin = timeToMinutes(row.drop_time);
          const priority = empDet.priority;
          const delay = priorityDelays[priority] || 0;
          const allowedDrop = empDet.latest_drop + delay;
          const earliestPickup = empDet.earliest_pickup;

          let earlyPickupViolation = pickupMin < earliestPickup;
          let lateDropViolation = dropMin > allowedDrop;

          if (!earlyPickupViolation && !lateDropViolation) {
            timeWindowSatisfied++;
          } else {
            if (earlyPickupViolation) {
              violations.push({
                employee_id: row.employee_id,
                type: "Time Window (Early Pickup)",
                expected: `earliest pickup: ${new Date(earliestPickup * 60000).toISOString().substr(11, 5)}`,
                actual: `${row.pickup_time}`,
              });
            }
            if (lateDropViolation) {
              let expectedDrop = `latest drop: ${new Date(empDet.latest_drop * 60000).toISOString().substr(11, 5)}`;
              if (delay > 0) {
                expectedDrop += ` (+${delay} min delay)`;
              }
              violations.push({
                employee_id: row.employee_id,
                type: "Time Window (Late Drop)",
                expected: expectedDrop,
                actual: `${row.drop_time}`,
              });
            }
          }
        });

        const compliance = [
          {
            label: "Vehicle Preference",
            current: vehPrefSatisfied,
            max: totalEmployees,
            percent: (vehPrefSatisfied / totalEmployees) * 100,
          },
          {
            label: "Sharing Preference",
            current: sharePrefSatisfied,
            max: totalEmployees,
            percent: (sharePrefSatisfied / totalEmployees) * 100,
          },
          {
            label: "Time Windows",
            current: timeWindowSatisfied,
            max: totalEmployees,
            percent: (timeWindowSatisfied / totalEmployees) * 100,
          },
        ];

        const computedOptimizedCost = totalOptimizedCostFromDistance.toFixed(1);

        const analytics = {
          totalEmployees,
          totalVehiclesUsed,
          totalVehiclesAvailable,
          totalDistance: totalDistance.toFixed(1),
          totalScore: totalScore,
          totalOptimizedCost: computedOptimizedCost,
          totalBaselineCost: totalBaselineCost.toFixed(1),
          totalOptimizedTime,
          totalBaselineTime,
          costSavings: costSave,
          timeSavings: timeSave,
          perVehicleCostData,
          employeeTimeComparison,
          unassignedVehicleIds,
          violations,
          compliance,
        };

        setStats({
          nodes: totalEmployees,
          routes: parsedRoutes.length,
          efficiency: `${totalDistance.toFixed(1)} km `,
          cost: `₹${computedOptimizedCost}`,
        });

        const pickupsWithId = [];
        const dropSet = new Set();
        const dropoffs = [];
        Object.entries(employeeDetails).forEach(([id, l]) => {
          if (l.pickup)
            pickupsWithId.push({ lat: l.pickup[0], lng: l.pickup[1], id });
          if (l.drop) {
            const key = `${l.drop[0]},${l.drop[1]}`;
            if (!dropSet.has(key)) {
              dropSet.add(key);
              dropoffs.push({ lat: l.drop[0], lng: l.drop[1] });
            }
          }
        });
if (onDataGenerated) {
  onDataGenerated({
    pickups: pickupsWithId,
    dropoffs,
    routes: parsedRoutes,
    rawAssignments: assignments,
    analytics,
    csvData: cleanCsvString,
    satisfactionInputs: {
      employeeDetails,
      vehicleDetailsMap,
      priorityDelays,
    }
  });

        }
        setStatusMsg("Ready.");
      } catch (err) {
        console.error("❌ ERROR:", err);
        setErrorMsg(err.message || "Processing Failed");
        setStatusMsg("Error");
      } finally {
        setLoading(false);
      }
    };
    reader.readAsArrayBuffer(uploadedFile);
  };

  return (
    <div className="flex flex-col h-full bg-slate-900 text-white p-4 gap-4 overflow-y-auto custom-scrollbar">
      <div className="flex items-center justify-between pb-2 border-b border-slate-700">
        <h2 className="text-lg font-bold flex items-center gap-2">
          <Settings className="w-4 h-4 text-cyan-400" />
          Velora Control
        </h2>
        <div className="flex items-center gap-3">
          {stats && outputCsv && (
            <button
              onClick={handleDownloadCSV}
              title="Download Route Data (CSV)"
              className="flex items-center gap-1.5 px-2 py-1 bg-emerald-500/10 border border-emerald-500/40 text-emerald-400 hover:bg-emerald-500/20 hover:text-emerald-300 rounded text-[10px] uppercase tracking-wider font-bold transition-colors"
            >
              <Download className="w-3 h-3" /> CSV
            </button>
          )}
          {loading && <RefreshCw className="w-4 h-4 animate-spin text-cyan-500" />}
        </div>
      </div>

      {errorMsg && (
        <div className="bg-red-900/20 border border-red-500/50 p-3 rounded text-xs text-red-200 flex flex-col gap-2">
          <div className="flex items-center gap-2 font-bold">
            <AlertCircle className="w-4 h-4" /> Error
          </div>
          <span className="opacity-80">{errorMsg}</span>
        </div>
      )}

      {stats ? (
        <div className="grid grid-cols-2 gap-3">
          <div className="bg-slate-800 p-3 rounded-lg border border-slate-700">
            <div className="text-slate-400 text-[10px] font-mono mb-1">
              EMPLOYEES
            </div>
            <div className="text-xl font-bold text-white flex items-center gap-2">
              <Users className="w-4 h-4 text-slate-500" />
              {stats.nodes}
            </div>
          </div>
          <div className="bg-slate-800 p-3 rounded-lg border border-slate-700">
            <div className="text-slate-400 text-[10px] font-mono mb-1">
              VEHICLES
            </div>
            <div className="text-xl font-bold text-emerald-400">
              {stats.routes}
            </div>
          </div>
          <div className="col-span-2 bg-gradient-to-r from-cyan-900/20 to-blue-900/20 p-3 rounded-lg border border-cyan-500/30 flex items-center justify-between">
            <div>
              <div className="text-cyan-300 text-[10px] font-mono mb-1">
                TOTAL FLEET DISTANCE
              </div>
              <div className="text-2xl font-bold text-white tracking-wider">
                {stats.efficiency}
              </div>
            </div>
            <Zap className="w-6 h-6 text-cyan-400" />
          </div>
        </div>
      ) : (
        <div className="p-4 border-2 border-dashed border-slate-700 rounded-lg flex flex-col items-center justify-center text-slate-500 gap-2 opacity-50 h-32">
          {loading ? (
            <Terminal className="w-6 h-6 animate-pulse" />
          ) : (
            <FileSpreadsheet className="w-6 h-6" />
          )}
          <span className="text-xs font-mono">{statusMsg}</span>
        </div>
      )}

      <div className="mt-auto flex flex-col gap-2">
        <button
          onClick={() => file && handleUploadAndOptimize(file)}
          disabled={loading || !file}
          className="w-full py-2 bg-slate-800 hover:bg-slate-700 disabled:bg-slate-800/50 text-slate-300 rounded text-xs font-bold transition-colors border border-slate-700"
        >
          {loading ? "PROCESSING..." : "RETRY CONNECTION"}
        </button>

        <button
          onClick={onReupload}
          className="w-full py-2 bg-cyan-600 hover:bg-cyan-500 rounded text-sm font-bold transition-colors shadow-lg shadow-cyan-900/20 flex items-center justify-center gap-2"
        >
          <Upload className="w-4 h-4" /> UPLOAD EXCEL
        </button>
      </div>
    </div>
  );
}
