import type { NextConfig } from "next";

const nextConfig: NextConfig = {
  async rewrites() {
    return [
      {
        source: '/api/backend/:path*',
        destination: 'http://35.208.133.51:5555/:path*',
      },
    ]
  },
};

export default nextConfig;
