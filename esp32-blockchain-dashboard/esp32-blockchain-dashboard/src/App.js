import React, { useState, useEffect } from 'react';
import { Activity, Database, Cpu, Wifi, HardDrive, Thermometer, Droplets, Wind, Battery, Clock, Server, Users, Box, Settings, Play, Pause, Trash2, Save } from 'lucide-react';
import { LineChart, Line, BarChart, Bar, XAxis, YAxis, CartesianGrid, Tooltip, Legend, ResponsiveContainer, PieChart, Pie, Cell } from 'recharts';

const ESP32BlockchainDashboard = () => {
  const [nodeData, setNodeData] = useState({
    address: '3C:61:05:12:34:56',
    role: 'VALIDATOR',
    blockCount: 0,
    totalBlocks: 0,
    txPoolCount: 0,
    txPoolSize: 20,
    peerCount: 0,
    uptime: 0,
    freeHeap: 245632,
    spiffsUsed: 0,
    spiffsTotal: 1441792
  });

  const [lastBlock, setLastBlock] = useState(null);
  const [telemetryData, setTelemetryData] = useState([]);
  const [transactions, setTransactions] = useState([]);
  const [blockHistory, setBlockHistory] = useState([]);
  const [performanceData, setPerformanceData] = useState([]);
  const [isConnected, setIsConnected] = useState(false);
  const [selectedTab, setSelectedTab] = useState('overview');
  const [backendUrl, setBackendUrl] = useState('http://localhost:5000');
  const [pusherClient, setPusherClient] = useState(null);

  // Initialize Pusher
  useEffect(() => {
    const script = document.createElement('script');
    script.src = 'https://js.pusher.com/8.2.0/pusher.min.js';
    script.async = true;
    script.onload = () => {
      const pusher = new window.Pusher('03904b6c0e72e6284b46', {
        cluster: 'ap1'
      });

      const channel = pusher.subscribe('blockchain');
      
      // Listen for new transactions
      channel.bind('new_transaction', (data) => {
        console.log('New transaction:', data);
        setTransactions(prev => [data, ...prev].slice(0, 20));
        setNodeData(prev => ({
          ...prev,
          txPoolCount: prev.txPoolCount + 1
        }));
      });

      // Listen for new blocks
      channel.bind('new_block', (data) => {
        console.log('New block:', data);
        setBlockHistory(prev => [...prev, {
          index: data.index,
          txCount: data.transactions.length,
          timestamp: new Date(data.timestamp).getTime(),
          size: data.size
        }].slice(-15));
        
        setLastBlock({
          index: data.index,
          txCount: data.transactions.length,
          hash: data.hash,
          timestamp: new Date(data.timestamp).getTime(),
          validator: data.validator
        });
        
        setNodeData(prev => ({
          ...prev,
          blockCount: prev.blockCount + 1,
          totalBlocks: prev.totalBlocks + 1,
          txPoolCount: Math.max(0, prev.txPoolCount - data.transactions.length)
        }));
      });

      // Listen for telemetry updates
      channel.bind('telemetry_update', (data) => {
        console.log('Telemetry update:', data);
        const telemetry = data.telemetry;
        setTelemetryData(prev => [...prev.slice(-19), {
          timestamp: new Date(telemetry.timestamp).getTime(),
          temperature: telemetry.temperature,
          humidity: telemetry.humidity,
          pressure: telemetry.pressure,
          battery: telemetry.battery
        }]);
      });

      // Listen for status updates
      channel.bind('status_update', (data) => {
        console.log('Status update:', data);
        setNodeData(prev => ({
          ...prev,
          blockCount: data.status.block_count || prev.blockCount,
          peerCount: data.status.peer_count || prev.peerCount,
          freeHeap: data.status.free_heap || prev.freeHeap,
          uptime: data.status.uptime || prev.uptime,
          spiffsUsed: data.status.spiffs_used || prev.spiffsUsed
        }));
      });

      // Listen for node registration
      channel.bind('node_registered', (data) => {
        console.log('Node registered:', data);
        setNodeData(prev => ({
          ...prev,
          address: data.node_id,
          role: data.data.role,
          peerCount: prev.peerCount + 1
        }));
        setIsConnected(true);
      });

      setPusherClient(pusher);
      setIsConnected(true);
    };

    document.body.appendChild(script);

    return () => {
      if (pusherClient) {
        pusherClient.disconnect();
      }
    };
  }, []);

  // Fetch initial data from backend
  useEffect(() => {
    const fetchInitialData = async () => {
      try {
        // Fetch dashboard data
        const dashResponse = await fetch(`${backendUrl}/api/dashboard`);
        const dashData = await dashResponse.json();
        
        if (dashData.node_data) {
          setNodeData(prev => ({
            ...prev,
            blockCount: dashData.block_count || 0,
            totalBlocks: dashData.block_count || 0,
            peerCount: dashData.peer_count || 0,
            txPoolCount: dashData.pending_transactions || 0
          }));
        }

        if (dashData.last_block) {
          setLastBlock({
            index: dashData.last_block.index,
            txCount: dashData.last_block.transactions?.length || 0,
            hash: dashData.last_block.hash,
            timestamp: new Date(dashData.last_block.timestamp).getTime(),
            validator: dashData.last_block.validator
          });
        }

        // Fetch transactions
        const txResponse = await fetch(`${backendUrl}/api/transactions`);
        const txData = await txResponse.json();
        setTransactions(txData.transactions || []);

        // Fetch blocks
        const blocksResponse = await fetch(`${backendUrl}/api/blocks`);
        const blocksData = await blocksResponse.json();
        setBlockHistory((blocksData.blocks || []).map(block => ({
          index: block.index,
          txCount: block.transactions?.length || 0,
          timestamp: new Date(block.timestamp).getTime(),
          size: block.size
        })));

      } catch (error) {
        console.error('Error fetching initial data:', error);
      }
    };

    fetchInitialData();
    const interval = setInterval(fetchInitialData, 10000); // Refresh every 10s

    return () => clearInterval(interval);
  }, [backendUrl]);

  // Update performance data
  useEffect(() => {
    const interval = setInterval(() => {
      setPerformanceData(prev => [...prev.slice(-9), {
        time: Date.now(),
        heap: nodeData.freeHeap,
        cpu: Math.random() * 40 + 20,
        network: Math.random() * 100
      }]);
    }, 3000);

    return () => clearInterval(interval);
  }, [nodeData.freeHeap]);

  const formatTime = (timestamp) => {
    const date = new Date(timestamp);
    return date.toLocaleTimeString();
  };

  const formatDuration = (seconds) => {
    const h = Math.floor(seconds / 3600);
    const m = Math.floor((seconds % 3600) / 60);
    const s = seconds % 60;
    return h + 'h ' + m + 'm ' + s + 's';
  };

  const formatBytes = (bytes) => {
    return (bytes / 1024).toFixed(1) + ' KB';
  };

  const changeRole = async (newRole) => {
    setNodeData(prev => ({ ...prev, role: newRole }));
    // You can add API call here to update role in backend
  };

  const clearStorage = () => {
    if (window.confirm('Clear all blockchain data? This cannot be undone.')) {
      setNodeData(prev => ({
        ...prev,
        blockCount: 0,
        totalBlocks: 0,
        txPoolCount: 0,
        spiffsUsed: 0
      }));
      setBlockHistory([]);
      setTransactions([]);
    }
  };

  const saveNow = () => {
    alert('Data saved to SPIFFS successfully!');
  };

  const COLORS = ['#3b82f6', '#10b981', '#f59e0b', '#ef4444', '#8b5cf6'];

  const roleColors = {
    VALIDATOR: 'bg-blue-500',
    SENSOR: 'bg-green-500',
    ARCHIVE: 'bg-purple-500'
  };

  const StatCard = ({ icon: Icon, label, value, subtitle, color }) => (
    <div className="bg-white rounded-lg shadow-md p-4 border-l-4" style={{ borderColor: color }}>
      <div className="flex items-center justify-between">
        <div className="flex-1">
          <div className="flex items-center gap-2 text-gray-600 text-sm mb-1">
            <Icon size={16} />
            <span>{label}</span>
          </div>
          <div className="text-2xl font-bold text-gray-800">{value}</div>
          {subtitle && <div className="text-xs text-gray-500 mt-1">{subtitle}</div>}
        </div>
      </div>
    </div>
  );

  return (
    <div className="min-h-screen bg-gradient-to-br from-gray-50 to-gray-100 p-4">
      <div className="bg-white rounded-lg shadow-lg p-6 mb-6">
        <div className="flex items-center justify-between mb-4">
          <div className="flex items-center gap-4">
            <div className="bg-blue-500 rounded-lg p-3">
              <Server className="text-white" size={32} />
            </div>
            <div>
              <h1 className="text-3xl font-bold text-gray-800">ESP32 Blockchain Node</h1>
              <div className="flex items-center gap-4 mt-1">
                <span className="text-sm text-gray-600">Address: {nodeData.address}</span>
                <span className={'px-3 py-1 rounded-full text-white text-sm font-semibold ' + roleColors[nodeData.role]}>
                  {nodeData.role}
                </span>
                <div className="flex items-center gap-1">
                  <div className={'w-2 h-2 rounded-full ' + (isConnected ? 'bg-green-500' : 'bg-red-500') + ' animate-pulse'} />
                  <span className="text-sm text-gray-600">{isConnected ? 'Connected to Backend' : 'Disconnected'}</span>
                </div>
              </div>
            </div>
          </div>
          <div className="flex gap-2">
            <input
              type="text"
              value={backendUrl}
              onChange={(e) => setBackendUrl(e.target.value)}
              placeholder="Backend URL"
              className="px-3 py-2 border-2 border-gray-300 rounded-lg focus:border-blue-500 focus:outline-none text-sm"
            />
            <button
              onClick={saveNow}
              className="px-4 py-2 bg-blue-500 hover:bg-blue-600 text-white rounded-lg font-semibold flex items-center gap-2 transition-colors"
            >
              <Save size={16} />
              Save
            </button>
          </div>
        </div>

        <div className="flex gap-2 border-b">
          {['overview', 'transactions', 'blocks', 'telemetry', 'performance'].map(tab => (
            <button
              key={tab}
              onClick={() => setSelectedTab(tab)}
              className={'px-4 py-2 font-semibold capitalize ' + (selectedTab === tab ? 'border-b-2 border-blue-500 text-blue-600' : 'text-gray-600 hover:text-gray-800')}
            >
              {tab}
            </button>
          ))}
        </div>
      </div>

      {selectedTab === 'overview' && (
        <>
          <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-4 gap-4 mb-6">
            <StatCard
              icon={Database}
              label="Blockchain"
              value={nodeData.blockCount}
              subtitle={'Total: ' + nodeData.totalBlocks + ' blocks'}
              color="#3b82f6"
            />
            <StatCard
              icon={Activity}
              label="Transaction Pool"
              value={nodeData.txPoolCount + '/' + nodeData.txPoolSize}
              subtitle={((nodeData.txPoolCount / nodeData.txPoolSize) * 100).toFixed(0) + '% full'}
              color="#10b981"
            />
            <StatCard
              icon={Users}
              label="Network Peers"
              value={nodeData.peerCount}
              subtitle="Connected nodes"
              color="#f59e0b"
            />
            <StatCard
              icon={Clock}
              label="Uptime"
              value={formatDuration(nodeData.uptime)}
              subtitle="System running"
              color="#8b5cf6"
            />
          </div>

          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6 mb-6">
            <div className="bg-white rounded-lg shadow-md p-6">
              <div className="flex items-center gap-2 mb-4">
                <Settings className="text-blue-500" size={24} />
                <h2 className="text-xl font-bold text-gray-800">Role Management</h2>
              </div>
              <div className="space-y-3">
                {['VALIDATOR', 'SENSOR', 'ARCHIVE'].map(role => (
                  <button
                    key={role}
                    onClick={() => changeRole(role)}
                    className={'w-full p-4 rounded-lg font-semibold text-left transition-all ' + (nodeData.role === role ? 'bg-blue-500 text-white shadow-lg scale-105' : 'bg-gray-100 text-gray-700 hover:bg-gray-200')}
                  >
                    <div className="flex items-center justify-between">
                      <div>
                        <div className="font-bold">{role}</div>
                        <div className="text-sm opacity-90">
                          {role === 'VALIDATOR' && 'Mines blocks and validates transactions'}
                          {role === 'SENSOR' && 'Collects and broadcasts telemetry data'}
                          {role === 'ARCHIVE' && 'Stores full blockchain history'}
                        </div>
                      </div>
                      {nodeData.role === role && (
                        <div className="w-4 h-4 bg-white rounded-full" />
                      )}
                    </div>
                  </button>
                ))}
              </div>
            </div>

            <div className="bg-white rounded-lg shadow-md p-6">
              <div className="flex items-center gap-2 mb-4">
                <Box className="text-purple-500" size={24} />
                <h2 className="text-xl font-bold text-gray-800">Last Block</h2>
              </div>
              {lastBlock ? (
                <div className="space-y-3">
                  <div className="flex justify-between items-center">
                    <span className="text-gray-600">Block #</span>
                    <span className="font-bold text-lg">{lastBlock.index}</span>
                  </div>
                  <div className="flex justify-between items-center">
                    <span className="text-gray-600">Transactions</span>
                    <span className="font-bold">{lastBlock.txCount}</span>
                  </div>
                  <div className="flex justify-between items-center">
                    <span className="text-gray-600">Hash</span>
                    <span className="font-mono text-sm">{lastBlock.hash}</span>
                  </div>
                  <div className="flex justify-between items-center">
                    <span className="text-gray-600">Time</span>
                    <span className="text-sm">{formatTime(lastBlock.timestamp)}</span>
                  </div>
                  <div className="flex justify-between items-center">
                    <span className="text-gray-600">Validator</span>
                    <span className="font-mono text-xs">{lastBlock.validator}</span>
                  </div>
                </div>
              ) : (
                <div className="text-gray-500 text-center py-8">No blocks mined yet</div>
              )}
            </div>
          </div>

          <div className="grid grid-cols-1 lg:grid-cols-3 gap-6 mb-6">
            <div className="bg-white rounded-lg shadow-md p-6">
              <div className="flex items-center gap-2 mb-4">
                <Cpu className="text-orange-500" size={24} />
                <h3 className="text-lg font-bold text-gray-800">Memory</h3>
              </div>
              <div className="text-3xl font-bold text-gray-800 mb-2">
                {formatBytes(nodeData.freeHeap)}
              </div>
              <div className="text-sm text-gray-600">Free heap memory</div>
              <div className="mt-4 h-2 bg-gray-200 rounded-full overflow-hidden">
                <div
                  className="h-full bg-gradient-to-r from-orange-400 to-orange-600"
                  style={{ width: ((nodeData.freeHeap / 300000) * 100) + '%' }}
                />
              </div>
            </div>

            <div className="bg-white rounded-lg shadow-md p-6">
              <div className="flex items-center gap-2 mb-4">
                <HardDrive className="text-indigo-500" size={24} />
                <h3 className="text-lg font-bold text-gray-800">Storage</h3>
              </div>
              <div className="text-3xl font-bold text-gray-800 mb-2">
                {((nodeData.spiffsUsed / nodeData.spiffsTotal) * 100).toFixed(1)}%
              </div>
              <div className="text-sm text-gray-600">
                {formatBytes(nodeData.spiffsUsed)} / {formatBytes(nodeData.spiffsTotal)}
              </div>
              <div className="mt-4 h-2 bg-gray-200 rounded-full overflow-hidden">
                <div
                  className="h-full bg-gradient-to-r from-indigo-400 to-indigo-600"
                  style={{ width: ((nodeData.spiffsUsed / nodeData.spiffsTotal) * 100) + '%' }}
                />
              </div>
            </div>

            <div className="bg-white rounded-lg shadow-md p-6">
              <div className="flex items-center gap-2 mb-4">
                <Wifi className="text-green-500" size={24} />
                <h3 className="text-lg font-bold text-gray-800">Network</h3>
              </div>
              <div className="text-3xl font-bold text-gray-800 mb-2">
                {nodeData.peerCount}
              </div>
              <div className="text-sm text-gray-600">Active connections</div>
              <button
                onClick={clearStorage}
                className="mt-4 w-full px-3 py-2 bg-red-500 hover:bg-red-600 text-white rounded-lg font-semibold flex items-center justify-center gap-2 transition-colors"
              >
                <Trash2 size={16} />
                Clear Storage
              </button>
            </div>
          </div>
        </>
      )}

      {selectedTab === 'transactions' && (
        <div className="bg-white rounded-lg shadow-md p-6">
          <h2 className="text-2xl font-bold text-gray-800 mb-4">Transaction Pool</h2>
          {transactions.length > 0 ? (
            <div className="overflow-x-auto">
              <table className="w-full">
                <thead>
                  <tr className="border-b-2 border-gray-200">
                    <th className="text-left py-3 px-4 font-semibold text-gray-700">Sensor ID</th>
                    <th className="text-left py-3 px-4 font-semibold text-gray-700">Temperature</th>
                    <th className="text-left py-3 px-4 font-semibold text-gray-700">Humidity</th>
                    <th className="text-left py-3 px-4 font-semibold text-gray-700">Time</th>
                    <th className="text-left py-3 px-4 font-semibold text-gray-700">Hash</th>
                    <th className="text-left py-3 px-4 font-semibold text-gray-700">Status</th>
                  </tr>
                </thead>
                <tbody>
                  {transactions.map(tx => (
                    <tr key={tx.id} className="border-b border-gray-100 hover:bg-gray-50">
                      <td className="py-3 px-4 font-mono text-sm">{tx.sensor_id}</td>
                      <td className="py-3 px-4">{tx.temperature?.toFixed(1)}°C</td>
                      <td className="py-3 px-4">{tx.humidity?.toFixed(1)}%</td>
                      <td className="py-3 px-4 text-sm text-gray-600">{formatTime(new Date(tx.timestamp).getTime())}</td>
                      <td className="py-3 px-4 font-mono text-xs">{tx.hash}</td>
                      <td className="py-3 px-4">
                        <span className={'px-2 py-1 rounded-full text-xs font-semibold ' + (tx.verified ? 'bg-green-100 text-green-800' : 'bg-yellow-100 text-yellow-800')}>
                          {tx.verified ? 'Verified' : 'Pending'}
                        </span>
                      </td>
                    </tr>
                  ))}
                </tbody>
              </table>
            </div>
          ) : (
            <div className="text-center py-12 text-gray-500">No transactions yet</div>
          )}
        </div>
      )}

      {selectedTab === 'blocks' && (
        <div className="bg-white rounded-lg shadow-md p-6">
          <h2 className="text-2xl font-bold text-gray-800 mb-4">Block History</h2>
          {blockHistory.length > 0 ? (
            <>
              <div className="mb-6">
                <ResponsiveContainer width="100%" height={300}>
                  <BarChart data={blockHistory}>
                    <CartesianGrid strokeDasharray="3 3" />
                    <XAxis dataKey="index" />
                    <YAxis />
                    <Tooltip />
                    <Legend />
                    <Bar dataKey="txCount" fill="#3b82f6" name="Transactions" />
                    <Bar dataKey="size" fill="#10b981" name="Size (bytes)" />
                  </BarChart>
                </ResponsiveContainer>
              </div>
              <div className="grid grid-cols-1 md:grid-cols-2 lg:grid-cols-3 gap-4">
                {blockHistory.slice(-6).reverse().map(block => (
                  <div key={block.index} className="border-2 border-gray-200 rounded-lg p-4 hover:shadow-lg transition-shadow">
                    <div className="flex items-center justify-between mb-2">
                      <span className="text-2xl font-bold text-blue-600">#{block.index}</span>
                      <span className="text-sm text-gray-500">{formatTime(block.timestamp)}</span>
                    </div>
                    <div className="space-y-1 text-sm">
                      <div className="flex justify-between">
                        <span className="text-gray-600">Transactions:</span>
                        <span className="font-semibold">{block.txCount}</span>
                      </div>
                      <div className="flex justify-between">
                        <span className="text-gray-600">Size:</span>
                        <span className="font-semibold">{block.size} bytes</span>
                      </div>
                    </div>
                  </div>
                ))}
              </div>
            </>
          ) : (
            <div className="text-center py-12 text-gray-500">No blocks mined yet</div>
          )}
        </div>
      )}

      {selectedTab === 'telemetry' && (
        <div className="space-y-6">
          {telemetryData.length > 0 ? (
            <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
              <div className="bg-white rounded-lg shadow-md p-6">
                <div className="flex items-center gap-2 mb-4">
                  <Thermometer className="text-red-500" size={24} />
                  <h2 className="text-xl font-bold text-gray-800">Temperature</h2>
                </div>
                <ResponsiveContainer width="100%" height={250}>
                  <LineChart data={telemetryData}>
                    <CartesianGrid strokeDasharray="3 3" />
                    <XAxis dataKey="timestamp" tickFormatter={formatTime} />
                    <YAxis domain={[15, 40]} />
                    <Tooltip labelFormatter={formatTime} />
                    <Line type="monotone" dataKey="temperature" stroke="#ef4444" strokeWidth={2} dot={false} />
                  </LineChart>
                </ResponsiveContainer>
              </div>

              <div className="bg-white rounded-lg shadow-md p-6">
                <div className="flex items-center gap-2 mb-4">
                  <Droplets className="text-blue-500" size={24} />
                  <h2 className="text-xl font-bold text-gray-800">Humidity</h2>
                </div>
                <ResponsiveContainer width="100%" height={250}>
                  <LineChart data={telemetryData}>
                    <CartesianGrid strokeDasharray="3 3" />
                    <XAxis dataKey="timestamp" tickFormatter={formatTime} />
                    <YAxis domain={[30, 90]} />
                    <Tooltip labelFormatter={formatTime} />
                    <Line type="monotone" dataKey="humidity" stroke="#3b82f6" strokeWidth={2} dot={false} />
                  </LineChart>
                </ResponsiveContainer>
              </div>

              <div className="bg-white rounded-lg shadow-md p-6">
                <div className="flex items-center gap-2 mb-4">
                  <Wind className="text-purple-500" size={24} />
                  <h2 className="text-xl font-bold text-gray-800">Pressure</h2>
                </div>
                <ResponsiveContainer width="100%" height={250}>
                  <LineChart data={telemetryData}>
                    <CartesianGrid strokeDasharray="3 3" />
                    <XAxis dataKey="timestamp" tickFormatter={formatTime} />
                    <YAxis domain={[990, 1030]} />
                    <Tooltip labelFormatter={formatTime} />
                    <Line type="monotone" dataKey="pressure" stroke="#8b5cf6" strokeWidth={2} dot={false} />
                  </LineChart>
                </ResponsiveContainer>
              </div>

              <div className="bg-white rounded-lg shadow-md p-6">
                <div className="flex items-center gap-2 mb-4">
                  <Battery className="text-green-500" size={24} />
                  <h2 className="text-xl font-bold text-gray-800">Battery</h2>
                </div>
                <ResponsiveContainer width="100%" height={250}>
                  <LineChart data={telemetryData}>
                    <CartesianGrid strokeDasharray="3 3" />
                    <XAxis dataKey="timestamp" tickFormatter={formatTime} />
                    <YAxis domain={[3.0, 4.3]} />
                    <Tooltip labelFormatter={formatTime} />
                    <Line type="monotone" dataKey="battery" stroke="#10b981" strokeWidth={2} dot={false} />
                  </LineChart>
                </ResponsiveContainer>
              </div>
            </div>
          ) : (
            <div className="bg-white rounded-lg shadow-md p-6">
              <div className="text-center py-12 text-gray-500">No telemetry data available</div>
            </div>
          )}
        </div>
      )}

      {selectedTab === 'performance' && (
        <div className="space-y-6">
          <div className="bg-white rounded-lg shadow-md p-6">
            <h2 className="text-2xl font-bold text-gray-800 mb-4">System Performance</h2>
            {performanceData.length > 0 ? (
              <ResponsiveContainer width="100%" height={300}>
                <LineChart data={performanceData}>
                  <CartesianGrid strokeDasharray="3 3" />
                  <XAxis dataKey="time" tickFormatter={formatTime} />
                  <YAxis yAxisId="left" />
                  <YAxis yAxisId="right" orientation="right" />
                  <Tooltip labelFormatter={formatTime} />
                  <Legend />
                  <Line yAxisId="left" type="monotone" dataKey="heap" stroke="#3b82f6" strokeWidth={2} name="Free Heap (bytes)" />
                  <Line yAxisId="right" type="monotone" dataKey="cpu" stroke="#10b981" strokeWidth={2} name="CPU (%)" />
                  <Line yAxisId="right" type="monotone" dataKey="network" stroke="#f59e0b" strokeWidth={2} name="Network (%)" />
                </LineChart>
              </ResponsiveContainer>
            ) : (
              <div className="text-center py-12 text-gray-500">No performance data available</div>
            )}
          </div>

          <div className="grid grid-cols-1 lg:grid-cols-2 gap-6">
            <div className="bg-white rounded-lg shadow-md p-6">
              <h3 className="text-xl font-bold text-gray-800 mb-4">Block Production Rate</h3>
              <div className="text-4xl font-bold text-blue-600 mb-2">
                {blockHistory.length > 0 && nodeData.uptime > 0 
                  ? (blockHistory.length / ((nodeData.uptime || 1) / 60)).toFixed(2)
                  : '0.00'}
              </div>
              <div className="text-gray-600">blocks per minute</div>
            </div>

            <div className="bg-white rounded-lg shadow-md p-6">
              <h3 className="text-xl font-bold text-gray-800 mb-4">Transaction Throughput</h3>
              <div className="text-4xl font-bold text-green-600 mb-2">
                {transactions.length > 0 && nodeData.uptime > 0
                  ? (transactions.length / ((nodeData.uptime || 1) / 60)).toFixed(2)
                  : '0.00'}
              </div>
              <div className="text-gray-600">transactions per minute</div>
            </div>
          </div>

          <div className="bg-white rounded-lg shadow-md p-6">
            <h3 className="text-xl font-bold text-gray-800 mb-4">Resource Distribution</h3>
            <ResponsiveContainer width="100%" height={300}>
              <PieChart>
                <Pie
                  data={[
                    { name: 'Blockchain Data', value: nodeData.spiffsUsed * 0.6 },
                    { name: 'Transaction Pool', value: nodeData.spiffsUsed * 0.25 },
                    { name: 'Metadata', value: nodeData.spiffsUsed * 0.1 },
                    { name: 'System', value: nodeData.spiffsUsed * 0.05 }
                  ]}
                  cx="50%"
                  cy="50%"
                  labelLine={false}
                  label={({ name, percent }) => `${name}: ${(percent * 100).toFixed(0)}%`}
                  outerRadius={100}
                  fill="#8884d8"
                  dataKey="value"
                >
                  {[0, 1, 2, 3].map((entry, index) => (
                    <Cell key={`cell-${index}`} fill={COLORS[index % COLORS.length]} />
                  ))}
                </Pie>
                <Tooltip />
              </PieChart>
            </ResponsiveContainer>
          </div>
        </div>
      )}
    </div>
  );
};

export default ESP32BlockchainDashboard;