# HCL / Terraform Highlight Test

Block types, attributes, strings with interpolation, comments, numbers and heredocs.

```hcl
resource "proxmox_virtual_environment_vm" "k3s_server" {
  count      = 3
  name       = "k3s-server-${count.index + 1}"
  node_name  = "pve"
  clone {
    vm_id = 10000   # id de la plantilla
  }
  cpu { cores = 2 }
  memory { dedicated = 2048 }
  network_device {
    bridge = "vmbr0"
    mac_address = "BC:24:11:AA:0${count.index + 1}:01"
  }
  disk {
    datastore_id = "local-zfs"
    size         = 32
  }
  initialization {
    ip_config {
      ipv4 {
        address = "192.168.1.4${count.index}/24"
        gateway = "192.168.1.1"
      }
    }
  }
}

variable "region" {
  description = "Región de despliegue"
  type        = string
  default     = "us-east-1"
}

output "server_ips" {
  value = [for vm in proxmox_virtual_environment_vm.k3s_server : vm.ipv4_addresses[0]]
}

data "template_file" "user_data" {
  template = <<EOT
#!/bin/bash
echo "hola ${var.region}"
EOT
}

terraform {
  required_version = ">= 1.5.0"   # comentario al final de línea
  backend "s3" {
    bucket = "tfstate"
    key    = "prod/terraform.tfstate"
  }
}

provider "aws" {
  region = var.region
  // comentario con doble barra
}

locals {
  common_tags = {
    Environment = "production"
    ManagedBy   = "terraform"
  }
  enabled = true
  replicas = 3
  ratio    = 0.75
  hex_mask = 0xFF
  big      = 1.5e3
  negative = -42
  types    = list(string)
}

/* comentario
   de bloque
   multilínea */
module "vpc" {
  source  = "terraform-aws-modules/vpc/aws"
  version = "5.0.0"
  name    = "main"
  azs     = ["us-east-1a", "us-east-1b"]
  enable_nat_gateway = false
  tags = merge(local.common_tags, { Project = "k3s" })
}

resource "null_resource" "provisioner" {
  provisioner "local-exec" {
    command = "echo done"
  }
}

resource "aws_instance" "web" {
  ami           = "ami-0c55b159cbfafe1f0"
  instance_type = "t3.micro"
  user_data     = <<-EOF
    #!/bin/bash
    apt-get update -y
  EOF
  lifecycle {
    create_before_destroy = true
  }
}

locals {
  filtered = [for s in var.services : s if s.enabled]
}
```
