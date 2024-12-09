package flora_tdma.simulations;

import inet.applications.udpapp.UdpBasicApp;
import inet.node.inet.Router;
import inet.node.internetcloud.InternetCloud;
import flora_tdma.LoRaPhy.LoRaMedium;
import flora_tdma.LoraNode.LoRaNode;
import flora_tdma.LoraNode.LoRaGW;
import inet.node.inet.StandardHost;
import inet.networklayer.configurator.ipv4.Ipv4NetworkConfigurator;
import inet.node.ethernet.Eth1G;

@license(LGPL);
network LoRaNetworkTest
{
    parameters:
        int numberOfNodes = default(1);
        int numberOfGateways = default(1);
        int networkSizeX = default(1000);
        int networkSizeY = default(1000);
        @display("bgb=887,596");
    submodules:
        loRaNodes[numberOfNodes]: LoRaNode {
            @display("p=400,304");
        }
        loRaGW[numberOfGateways]: LoRaGW {
            @display("p=100,100;is=s");
        }
        LoRaMedium: LoRaMedium {
            @display("p=300,200");
        }
    connections:
}

