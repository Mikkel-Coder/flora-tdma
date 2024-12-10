//
// This program is free software: you can redistribute it and/or modify
// it under the terms of the GNU Lesser General Public License as published by
// the Free Software Foundation, either version 3 of the License, or
// (at your option) any later version.
// 
// This program is distributed in the hope that it will be useful,
// but WITHOUT ANY WARRANTY; without even the implied warranty of
// MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
// GNU Lesser General Public License for more details.
// 
// You should have received a copy of the GNU Lesser General Public License
// along with this program.  If not, see http://www.gnu.org/licenses/.
// 

package flora_tdma.LoraNode;

import inet.node.contract.INetworkNode;
import inet.mobility.static.StationaryMobility;
import inet.networklayer.common.InterfaceTable;
import inet.networklayer.contract.INetworkLayer;
import inet.linklayer.loopback.LoopbackInterface;
import flora_tdma.LoRa.LoRaGWNic;
import flora_tdma.LoRaApp.SimpleLoRaApp;
import inet.linklayer.contract.IEthernetInterface;
import inet.applications.contract.IApp;
import inet.transportlayer.contract.IUdp;
import inet.networklayer.contract.IRoutingTable;
import inet.common.MessageDispatcher;
import inet.protocolelement.contract.IProtocolLayer;

module LoRaGW like INetworkNode
{
    parameters:
        @networkNode();
        *.interfaceTableModule = default(absPath(".interfaceTable"));
        *.energySourceModule = default(exists(energyStorage) ? absPath(".energyStorage") : "");

        int numEthInterfaces = default(0);  // minimum number of ethernet interfaces
        int numWlanInterfaces = 1;

        bool hasIpv4 = default(true);
        string networkLayerType = default("Ipv4NetworkLayer");
        string routingTableType = default("Ipv4RoutingTable");
        int numUdpApps = default(0);
        bool hasUdp = default(firstAvailableOrEmpty("Udp") != "");
        string udpType = default(firstAvailableOrEmpty("UDP"));

        //LoRaGWNic.radio.antenna.mobilityModule = default("^.^.^.mobility");

        *.mobilityModule = default(exists(mobility) ? absPath(".mobility") : "");

        ethernet.registerProtocol = default(true);

        @display("bgb=783.96,587.688;i=device/antennatower");
    gates:
        inout ethg[numEthInterfaces] @labels(EtherFrame-conn) @allowUnconnected;
        input radioIn[numWlanInterfaces] @directIn;

    submodules:
        mobility: StationaryMobility {
            @display("p=135.36,199.656");
        }
        LoRaGWNic: LoRaGWNic {
            @display("p=626.04004,94.75201");
        }
    connections allowunconnected:
}
