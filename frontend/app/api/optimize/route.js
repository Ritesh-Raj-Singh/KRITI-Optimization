import { NextResponse } from 'next/server';
import * as XLSX from 'xlsx';
import fs from 'fs';
import path from 'path';

export async function POST(req) {
  try {
    console.log("📂 Receiving file upload...");

    // 1. Parse the Uploaded File
    const formData = await req.formData();
    const file = formData.get('file');

    if (!file) {
      return NextResponse.json({ error: "No file uploaded" }, { status: 400 });
    }

    // Convert file to buffer for reading
    const bytes = await file.arrayBuffer();
    const buffer = Buffer.from(bytes);
    const workbook = XLSX.read(buffer, { type: 'buffer' });

    console.log(`✅ File read successfully. Sheets found: ${workbook.SheetNames.join(', ')}`);

    // 2. Define Output Directory (backend/data)
    // This path is relative to your project root in VS Code
    const dataDir = path.join(process.cwd(), 'backend', 'data');

    // Create folder if it doesn't exist
    if (!fs.existsSync(dataDir)) {
      fs.mkdirSync(dataDir, { recursive: true });
      console.log("📁 Created directory: backend/data/");
    }

    // 3. Extract Specific Sheets & Save as CSV
    const targetSheets = ['employees', 'vehicles', 'baseline', 'metadata'];
    const createdFiles = [];

    targetSheets.forEach(sheetName => {
      // Find the sheet (case-insensitive search)
      const exactSheetName = workbook.SheetNames.find(name => 
        name.toLowerCase().trim() === sheetName.toLowerCase()
      );

      if (exactSheetName) {
        const worksheet = workbook.Sheets[exactSheetName];
        const csvData = XLSX.utils.sheet_to_csv(worksheet);

        // Define the file path
        const filePath = path.join(dataDir, `${sheetName}.csv`);

        // Write the file to your VS Code project folder
        fs.writeFileSync(filePath, csvData);
        createdFiles.push(`${sheetName}.csv`);
        
        console.log(`📝 Saved: ${filePath}`);
      } else {
        console.warn(`⚠️ Warning: Sheet '${sheetName}' not found in Excel file.`);
      }
    });

    // 4. Return Success Response
    if (createdFiles.length === 0) {
      return NextResponse.json({ 
        error: "No matching sheets found. Check your Excel sheet names." 
      }, { status: 400 });
    }

    return NextResponse.json({ 
      success: true, 
      message: "CSVs generated successfully", 
      files: createdFiles,
      location: dataDir
    });

  } catch (error) {
    console.error("❌ Error processing file:", error);
    return NextResponse.json({ error: error.message }, { status: 500 });
  }
}