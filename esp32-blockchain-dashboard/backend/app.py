from dotenv import load_dotenv
load_dotenv()  # ✅ MUST be before importing Config

from flask import Flask, request, jsonify
from flask_cors import CORS
import pusher
import json
import time
from datetime import datetime
import hashlib
import random
import os

from config import Config

print("APP ID:", Config.PUSHER_APP_ID)




app = Flask(__name__)
CORS(app)

# Pusher Configuration
pusher_client = pusher.Pusher(
    app_id=Config.PUSHER_APP_ID,
    key=Config.PUSHER_KEY,
    secret=Config.PUSHER_SECRET,
    cluster=Config.PUSHER_CLUSTER,
    ssl=True
)

# In-memory storage (replace with database in production)
blockchain_data = {
    'nodes': {},
    'transactions': [],
    'blocks': [],
    'telemetry': []
}

# ESP32 node registry
@app.route('/api/register', methods=['POST'])
def register_node():
    data = request.json
    node_id = data.get('mac_address', data.get('node_id'))
    
    if not node_id:
        return jsonify({'error': 'MAC address or node ID required'}), 400
    
    blockchain_data['nodes'][node_id] = {
        'role': data.get('role', 'SENSOR'),
        'last_seen': datetime.now().isoformat(),
        'ip_address': request.remote_addr,
        'status': 'online',
        'block_count': 0,
        'peer_count': 0,
        'free_heap': 0,
        'uptime': 0
    }
    
    # Notify frontend
    pusher_client.trigger('blockchain', 'node_registered', {
        'node_id': node_id,
        'data': blockchain_data['nodes'][node_id]
    })
    
    return jsonify({
        'status': 'registered',
        'node_id': node_id,
        'assigned_role': blockchain_data['nodes'][node_id]['role']
    })

# Receive telemetry data from ESP32
@app.route('/api/telemetry', methods=['POST'])
def receive_telemetry():
    data = request.json
    node_id = data.get('node_id')
    
    if not node_id or node_id not in blockchain_data['nodes']:
        return jsonify({'error': 'Node not registered'}), 400
    
    # Create transaction from telemetry data
    transaction = {
        'id': hashlib.md5(f"{node_id}{time.time()}".encode()).hexdigest()[:16],
        'sensor_id': node_id,
        'temperature': data.get('temperature', round(random.uniform(20, 35), 1)),
        'humidity': data.get('humidity', round(random.uniform(40, 80), 1)),
        'pressure': data.get('pressure', round(random.uniform(1000, 1020), 1)),
        'battery': data.get('battery', round(random.uniform(3.2, 4.2), 2)),
        'timestamp': datetime.now().isoformat(),
        'hash': hashlib.sha256(f"{node_id}{time.time()}".encode()).hexdigest()[:20] + '...',
        'verified': False,
        'type': 'TELEMETRY'
    }
    
    blockchain_data['transactions'].insert(0, transaction)
    blockchain_data['transactions'] = blockchain_data['transactions'][:50]  # Keep last 50
    
    # Update node status
    blockchain_data['nodes'][node_id].update({
        'last_seen': datetime.now().isoformat(),
        'free_heap': data.get('free_heap', 0),
        'uptime': data.get('uptime', 0)
    })
    
    # Notify frontend
    pusher_client.trigger('blockchain', 'new_transaction', transaction)
    pusher_client.trigger('blockchain', 'telemetry_update', {
        'node_id': node_id,
        'telemetry': transaction
    })
    
    return jsonify({'status': 'received', 'transaction_id': transaction['id']})

# ESP32 status update
@app.route('/api/status', methods=['POST'])
def update_status():
    data = request.json
    node_id = data.get('node_id')
    
    if node_id in blockchain_data['nodes']:
        blockchain_data['nodes'][node_id].update({
            'block_count': data.get('block_count', 0),
            'peer_count': data.get('peer_count', 0),
            'free_heap': data.get('free_heap', 0),
            'uptime': data.get('uptime', 0),
            'spiffs_used': data.get('spiffs_used', 0),
            'spiffs_total': data.get('spiffs_total', 1441792),
            'last_seen': datetime.now().isoformat()
        })
        
        pusher_client.trigger('blockchain', 'status_update', {
            'node_id': node_id,
            'status': blockchain_data['nodes'][node_id]
        })
    
    return jsonify({'status': 'updated'})

# Mine new block (for validator nodes)
@app.route('/api/mine', methods=['POST'])
def mine_block():
    data = request.json
    node_id = data.get('node_id')
    
    if node_id not in blockchain_data['nodes']:
        return jsonify({'error': 'Node not registered'}), 400
    
    # Get pending transactions
    pending_tx = [tx for tx in blockchain_data['transactions'][:10] if not tx.get('verified')]
    
    if not pending_tx:
        return jsonify({'error': 'No pending transactions'}), 400
    
    # Create new block
    new_block = {
        'index': len(blockchain_data['blocks']),
        'timestamp': datetime.now().isoformat(),
        'transactions': pending_tx[:4],  # Limit transactions per block
        'validator': node_id,
        'previous_hash': blockchain_data['blocks'][-1]['hash'] if blockchain_data['blocks'] else '0',
        'hash': hashlib.sha256(f"{len(blockchain_data['blocks'])}{time.time()}".encode()).hexdigest()[:20] + '...',
        'size': sum(len(str(tx)) for tx in pending_tx[:4])
    }
    
    # Mark transactions as verified
    for tx in pending_tx[:4]:
        tx['verified'] = True
    
    blockchain_data['blocks'].append(new_block)
    
    # Update node block count
    blockchain_data['nodes'][node_id]['block_count'] += 1
    
    # Notify frontend
    pusher_client.trigger('blockchain', 'new_block', new_block)
    
    return jsonify({
        'status': 'mined',
        'block': new_block,
        'transactions_processed': len(pending_tx[:4])
    })

# Frontend API endpoints
@app.route('/api/dashboard', methods=['GET'])
def get_dashboard():
    node_id = request.args.get('node_id')
    
    if node_id and node_id in blockchain_data['nodes']:
        node_data = blockchain_data['nodes'][node_id]
    else:
        # Return first node or default data
        node_data = next(iter(blockchain_data['nodes'].values()), {})
    
    return jsonify({
        'node_data': node_data,
        'last_block': blockchain_data['blocks'][-1] if blockchain_data['blocks'] else None,
        'transaction_count': len(blockchain_data['transactions']),
        'pending_transactions': len([tx for tx in blockchain_data['transactions'] if not tx.get('verified')]),
        'block_count': len(blockchain_data['blocks']),
        'peer_count': len(blockchain_data['nodes'])
    })

@app.route('/api/transactions', methods=['GET'])
def get_transactions():
    return jsonify({
        'transactions': blockchain_data['transactions'][:20]
    })

@app.route('/api/blocks', methods=['GET'])
def get_blocks():
    return jsonify({
        'blocks': blockchain_data['blocks'][-15:]  # Last 15 blocks
    })

@app.route('/api/nodes', methods=['GET'])
def get_nodes():
    return jsonify({
        'nodes': blockchain_data['nodes']
    })

# Health check
@app.route('/api/health', methods=['GET'])
def health_check():
    return jsonify({
        'status': 'healthy',
        'timestamp': datetime.now().isoformat(),
        'nodes_count': len(blockchain_data['nodes']),
        'blocks_count': len(blockchain_data['blocks']),
        'transactions_count': len(blockchain_data['transactions'])
    })

if __name__ == '__main__':
    app.run(host='0.0.0.0', port=5000, debug=True)